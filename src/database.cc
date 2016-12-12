/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#include <node.h>
#include <node_buffer.h>
#include <sys/stat.h>
#include <nan.h>

#include "leveldown.h"
#include "database.h"
#include "async.h"
#include "database_async.h"
#include "batch.h"
#include "iterator.h"
#include "common.h"

#include <string.h>

namespace leveldown {

static Nan::Persistent<v8::FunctionTemplate> database_constructor;

#if (NODE_MODULE_VERSION > 0x000B)
  typedef uv_stat_t * __uv_stat__;
#else
  typedef uv_statbuf_t * __uv_stat__;
#endif

inline __uv_stat__ Stat (const char* path) {
  uv_fs_t req;
  int result = uv_fs_lstat(uv_default_loop(), &req, path, NULL);
  if (result < 0)
    return NULL;
  return static_cast<const __uv_stat__>(req.ptr);
}

inline bool IsDirectory (const __uv_stat__ stat) {
  return (stat->st_mode & S_IFMT) == S_IFDIR;
}

inline bool MakeDirectory (const char* path) {
  uv_fs_t req;
  return uv_fs_mkdir(uv_default_loop(), &req, path, 511, NULL);
}

Database::Database (const v8::Local<v8::Value>& from)
  : location(new Nan::Utf8String(from))
  , currentIteratorId(0)
  , pendingCloseWorker(NULL)
{};

Database::~Database () {
  delete location;
};

/* Calls from worker threads, NO V8 HERE *****************************/

md_status Database::OpenDatabase (OpenOptions options) {
  md_status status;

  // Emulate the behaviour of LevelDB create_if_missing & error_if_exists
  // options, with an additional check for stat == directory
  const __uv_stat__ stat = Stat(**location);
  if (options.noSubdir) {
    if (stat == NULL) {
      if (!options.createIfMissing) {
        status.error = std::string(**location);
        status.error += " does not exist (createIfMissing is false)";
        return status;
      }
    } else {
      if (options.errorIfExists) {
        status.error = std::string(**location);
        status.error += " exists (errorIfExists is true)";
        return status;
      }
    }
  } else {
    if (stat == NULL) {
      if (options.createIfMissing) {
        status.code = MakeDirectory(**location);
        if (status.code) {
          status.error = std::string(**location);
          status.error += " cannot be created (createIfMissing is true)";
          return status;
        }
      } else {
        status.error = std::string(**location);
        status.error += " does not exist (createIfMissing is false)";
        return status;
      }
    } else {
      if (!IsDirectory(stat)) {
        status.error = std::string(**location);
        status.error += " exists and is not a directory";
        return status;
      }
      if (options.errorIfExists) {
        status.error = std::string(**location);
        status.error += " exists (errorIfExists is true)";
        return status;
      }
    }
  }

  int env_opt = 0;
  int txn_opt = 0;

  if (!options.sync)
    env_opt |= MDB_NOSYNC;

  if (options.readOnly) {
    env_opt |= MDB_RDONLY;
    txn_opt |= MDB_RDONLY;
  }

  if (options.writeMap)
    env_opt |= MDB_WRITEMAP;

  if (!options.metaSync)
    env_opt |= MDB_NOMETASYNC;

  if (options.mapAsync)
    env_opt |= MDB_MAPASYNC;

  if (options.fixedMap)
    env_opt |= MDB_FIXEDMAP;

  if (!options.notls)
    env_opt |= MDB_NOTLS;

  if (options.noSubdir)
    env_opt |= MDB_NOSUBDIR;

  status.code = mdb_env_create(&env);
  if (status.code)
    return status;

  status.code = mdb_env_set_mapsize(env, options.mapSize);
  if (status.code) {
    mdb_env_close(env);
    return status;
  }

  status.code = mdb_env_set_maxreaders(env, options.maxReaders);
  if (status.code) {
    mdb_env_close(env);
    return status;
  }

  status.code = mdb_env_open(env, **location, env_opt, 0664);
  if (status.code) {
    mdb_env_close(env);
    return status;
  }

  MDB_txn *txn;

  status.code = mdb_txn_begin(env, NULL, txn_opt, &txn);
  if (status.code) {
    mdb_env_close(env);
    return status;
  }

  status.code = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (status.code) {
    mdb_txn_abort(txn);
    mdb_env_close(env);
    return status;
  }

  status.code = mdb_txn_commit(txn);

  if (status.code) {
    mdb_env_close(env);
    return status;
  }

  return status;
}

void Database::CloseDatabase () {
  mdb_env_close(env);
}

int Database::PutToDatabase (MDB_val key, MDB_val value) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;

  rc = mdb_put(txn, dbi, &key, &value, 0);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }

  rc = mdb_txn_commit(txn);

  return rc;
}

int Database::PutToDatabase (std::vector< BatchOp* >* operations) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;

  for (std::vector< BatchOp* >::iterator it = operations->begin()
      ; it != operations->end()
      ; it++) {

    rc = (*it)->Execute(txn, dbi);
    if (rc != 0 && rc != MDB_NOTFOUND) {
      mdb_txn_abort(txn);
      return rc;
    }
  }

  rc = mdb_txn_commit(txn);

  return rc;
}

int Database::GetFromDatabase (MDB_val key, std::string& value) {
  int rc;
  MDB_txn *txn;
  MDB_val val;

  rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
  if (rc)
    return rc;

  rc = mdb_get(txn, dbi, &key, &val);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }

  // We need to copy the data before the txn
  // is committed, lest we end up with a nasty
  // race condition on the next update.
  value.assign((char*)val.mv_data, val.mv_size);

  rc = mdb_txn_commit(txn);

  return rc;
}

int Database::DeleteFromDatabase (MDB_val key) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;

  rc = mdb_del(txn, dbi, &key, NULL);
  if (rc != 0 && rc != MDB_NOTFOUND) {
    mdb_txn_abort(txn);
    return rc;
  }

  rc = mdb_txn_commit(txn);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }

  return rc;
}

int Database::NewCursor (MDB_txn **txn, MDB_cursor **cursor) {
  int rc;

  rc = mdb_txn_begin(env, NULL, MDB_RDONLY, txn);
  if (rc)
    return rc;

  rc = mdb_cursor_open(*txn, dbi, cursor);
  if (rc) {
    mdb_txn_abort(*txn);
    return rc;
  }

  return rc;
}

uint64_t Database::ApproximateSizeFromDatabase (MDB_val* start, MDB_val* end) {
  uint64_t size = 0;
  int rc;
  MDB_txn* txn;
  MDB_cursor* cursor;
  MDB_val key;
  MDB_val val;

  rc = NewCursor(&txn, &cursor);

  if (rc != 0)
    return size;

  if (start != NULL) {
    key.mv_data = start->mv_data;
    key.mv_size = start->mv_size;
    rc = mdb_cursor_get(cursor, &key, &val, MDB_SET_RANGE);
  } else {
    rc = mdb_cursor_get(cursor, &key, &val, MDB_FIRST);
  }

  while (rc == 0) {
    size += key.mv_size;
    size += val.mv_size;
    if (end != NULL && mdb_cmp(txn, dbi, &key, end) >= 0)
      break;
    rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT);
  }

  mdb_cursor_close(cursor);
  mdb_txn_abort(txn);

  return size;
}

int Database::BackupDatabase (char* path) {
  int rc = 0;
  unsigned int flags;

  rc = mdb_env_get_flags(env, &flags);

  if (rc != 0)
    return rc;

  if (!(flags & MDB_NOSUBDIR)) {
    const __uv_stat__ stat = Stat(path);

    if (stat == NULL)
      rc = (int)MakeDirectory(path);

    if (rc != 0)
      return rc;
  }

  return mdb_env_copy(env, path);
}

void Database::GetPropertyFromDatabase (
      char* property
    , std::string* value) {

  if (strcmp(property, "mdb.version") == 0) {
    char *version = mdb_version(NULL, NULL, NULL);
    if (version != NULL)
      value->assign((const char*)version);
    return;
  }

  MDB_envinfo info;
  int ret = mdb_env_info(env, &info);

  if (ret != 0)
    return;

  if (strcmp(property, "mdb.mapsize") == 0) {
    std::string s = std::to_string((long long)info.me_mapsize);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.last_pgno") == 0) {
    std::string s = std::to_string((long long)info.me_last_pgno);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.last_txnid") == 0) {
    std::string s = std::to_string((long long)info.me_last_txnid);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.maxreaders") == 0) {
    std::string s = std::to_string(info.me_maxreaders);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.numreaders") == 0) {
    std::string s = std::to_string(info.me_numreaders);
    value->assign(s.data(), s.size());
    return;
  }

  MDB_stat stat;
  ret = mdb_env_stat(env, &stat);

  if (ret != 0)
    return;

  if (strcmp(property, "mdb.psize") == 0) {
    std::string s = std::to_string(stat.ms_psize);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.depth") == 0) {
    std::string s = std::to_string(stat.ms_depth);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.branch_pages") == 0) {
    std::string s = std::to_string((long long)stat.ms_branch_pages);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.leaf_pages") == 0) {
    std::string s = std::to_string((long long)stat.ms_leaf_pages);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.overflow_pages") == 0) {
    std::string s = std::to_string((long long)stat.ms_overflow_pages);
    value->assign(s.data(), s.size());
    return;
  }

  if (strcmp(property, "mdb.entries") == 0) {
    std::string s = std::to_string((long long)stat.ms_entries);
    value->assign(s.data(), s.size());
    return;
  }
}

void Database::ReleaseIterator (uint32_t id) {
  // called each time an Iterator is End()ed, in the main thread
  // we have to remove our reference to it and if it's the last iterator
  // we have to invoke a pending CloseWorker if there is one
  // if there is a pending CloseWorker it means that we're waiting for
  // iterators to end before we can close them
  iterators.erase(id);
  if (iterators.empty() && pendingCloseWorker != NULL) {
    Nan::AsyncQueueWorker((AsyncWorker*)pendingCloseWorker);
    pendingCloseWorker = NULL;
  }
}

/* V8 exposed functions *****************************/

NAN_METHOD(LevelDOWN) {
  v8::Local<v8::String> location = info[0].As<v8::String>();
  info.GetReturnValue().Set(Database::NewInstance(location));
}

void Database::Init () {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(Database::New);
  database_constructor.Reset(tpl);
  tpl->SetClassName(Nan::New("Database").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetPrototypeMethod(tpl, "open", Database::Open);
  Nan::SetPrototypeMethod(tpl, "close", Database::Close);
  Nan::SetPrototypeMethod(tpl, "put", Database::Put);
  Nan::SetPrototypeMethod(tpl, "get", Database::Get);
  Nan::SetPrototypeMethod(tpl, "del", Database::Delete);
  Nan::SetPrototypeMethod(tpl, "batch", Database::Batch);
  Nan::SetPrototypeMethod(tpl, "approximateSize", Database::ApproximateSize);
  Nan::SetPrototypeMethod(tpl, "getProperty", Database::GetProperty);
  Nan::SetPrototypeMethod(tpl, "backup", Database::Backup);
  Nan::SetPrototypeMethod(tpl, "iterator", Database::Iterator);
}

NAN_METHOD(Database::New) {
  Database* obj = new Database(info[0]);
  obj->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

v8::Local<v8::Value> Database::NewInstance (v8::Local<v8::String> &location) {
  Nan::EscapableHandleScope scope;

  Nan::MaybeLocal<v8::Object> maybeInstance;
  v8::Local<v8::Object> instance;

  v8::Local<v8::FunctionTemplate> constructorHandle =
      Nan::New<v8::FunctionTemplate>(database_constructor);

  v8::Local<v8::Value> argv[] = { location };
  maybeInstance = Nan::NewInstance(constructorHandle->GetFunction(), 1, argv);

  if (maybeInstance.IsEmpty())
    Nan::ThrowError("Could not create new Database instance");
  else
    instance = maybeInstance.ToLocalChecked();

  return scope.Escape(instance);
}

NAN_METHOD(Database::Open) {
  LD_METHOD_SETUP_COMMON(open, 0, 1)

  OpenOptions options;

  options.createIfMissing = BooleanOptionValue(
      optionsObj
    , "createIfMissing"
    , true
  );
  options.errorIfExists = BooleanOptionValue(
      optionsObj
    , "errorIfExists"
  , false);
  options.mapSize = UInt64OptionValue(
      optionsObj
    , "mapSize"
    , DEFAULT_MAPSIZE
  );
  options.maxReaders = UInt64OptionValue(
      optionsObj
    , "maxReaders"
    , DEFAULT_READERS
  );
  options.sync = BooleanOptionValue(
      optionsObj
    , "sync"
  , DEFAULT_SYNC);
  options.readOnly = BooleanOptionValue(
      optionsObj
    , "readOnly"
    , DEFAULT_READONLY
  );
  options.writeMap = BooleanOptionValue(
      optionsObj
    , "writeMap"
    , DEFAULT_WRITEMAP
  );
  options.metaSync = BooleanOptionValue(
      optionsObj
    , "metaSync"
    , DEFAULT_METASYNC
  );
  options.mapAsync = BooleanOptionValue(
      optionsObj
    , "mapAsync"
    , DEFAULT_MAPASYNC
  );
  options.fixedMap = BooleanOptionValue(
      optionsObj
    , "fixedMap"
    , DEFAULT_FIXEDMAP
  );
  options.notls = BooleanOptionValue(
      optionsObj
    , "notls"
    , DEFAULT_NOTLS
  );
  options.noSubdir = BooleanOptionValue(
      optionsObj
    , "noSubdir"
    , DEFAULT_NOSUBDIR
  );

  OpenWorker* worker = new OpenWorker(
      database
    , new Nan::Callback(callback)
    , options
  );

  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);

  Nan::AsyncQueueWorker(worker);
}

// for an empty callback to iterator.end()
NAN_METHOD(EmptyMethod) {
}

NAN_METHOD(Database::Close) {
  LD_METHOD_SETUP_COMMON_ONEARG(close)

  CloseWorker* worker = new CloseWorker(
      database
    , new Nan::Callback(callback)
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);

  if (!database->iterators.empty()) {
    // yikes, we still have iterators open! naughty naughty.
    // we have to queue up a CloseWorker and manually close each of them.
    // the CloseWorker will be invoked once they are all cleaned up
    database->pendingCloseWorker = worker;

    for (
        std::map< uint32_t, leveldown::Iterator * >::iterator it
            = database->iterators.begin()
      ; it != database->iterators.end()
      ; ++it) {

        // for each iterator still open, first check if it's already in
        // the process of ending (ended==true means an async End() is
        // in progress), if not, then we call End() with an empty callback
        // function and wait for it to hit ReleaseIterator() where our
        // CloseWorker will be invoked

        leveldown::Iterator *iterator = it->second;

        if (!iterator->ended) {
          v8::Local<v8::Function> end =
              v8::Local<v8::Function>::Cast(iterator->handle()->Get(
                  Nan::New<v8::String>("end").ToLocalChecked()));
          v8::Local<v8::Value> argv[] = {
              Nan::New<v8::FunctionTemplate>(EmptyMethod)->GetFunction() // empty callback
          };
          Nan::MakeCallback(
              iterator->handle()
            , end
            , 1
            , argv
          );
        }
    }
  } else {
    Nan::AsyncQueueWorker(worker);
  }
}

NAN_METHOD(Database::Put) {
  LD_METHOD_SETUP_COMMON(put, 2, 3)

  v8::Local<v8::Object> keyHandle = info[0].As<v8::Object>();
  v8::Local<v8::Object> valueHandle = info[1].As<v8::Object>();
  LD_STRING_OR_BUFFER_TO_SLICE(key, keyHandle, key);
  LD_STRING_OR_BUFFER_TO_SLICE(value, valueHandle, value);

  bool sync = BooleanOptionValue(optionsObj, "sync");

  WriteWorker* worker = new WriteWorker(
      database
    , new Nan::Callback(callback)
    , key
    , value
    , sync
    , keyHandle
    , valueHandle
  );

  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(Database::Get) {
  LD_METHOD_SETUP_COMMON(get, 1, 2)

  v8::Local<v8::Object> keyHandle = info[0].As<v8::Object>();
  LD_STRING_OR_BUFFER_TO_SLICE(key, keyHandle, key);

  bool asBuffer = BooleanOptionValue(optionsObj, "asBuffer", true);
  bool fillCache = BooleanOptionValue(optionsObj, "fillCache", true);

  ReadWorker* worker = new ReadWorker(
      database
    , new Nan::Callback(callback)
    , key
    , asBuffer
    , fillCache
    , keyHandle
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(Database::Delete) {
  LD_METHOD_SETUP_COMMON(del, 1, 2)

  v8::Local<v8::Object> keyHandle = info[0].As<v8::Object>();
  LD_STRING_OR_BUFFER_TO_SLICE(key, keyHandle, key);

  bool sync = BooleanOptionValue(optionsObj, "sync");

  DeleteWorker* worker = new DeleteWorker(
      database
    , new Nan::Callback(callback)
    , key
    , sync
    , keyHandle
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(Database::Batch) {
  if ((info.Length() == 0 || info.Length() == 1) && !info[0]->IsArray()) {
    v8::Local<v8::Object> optionsObj;
    if (info.Length() > 0 && info[0]->IsObject()) {
      optionsObj = info[0].As<v8::Object>();
    }
    info.GetReturnValue().Set(WriteBatch::NewInstance(info.This(), optionsObj));
    return;
  }

  LD_METHOD_SETUP_COMMON(batch, 1, 2)

  bool sync = BooleanOptionValue(optionsObj, "sync");

  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(info[0]);

  WriteBatch* batch = new WriteBatch(database, sync);

  for (unsigned int i = 0; i < array->Length(); i++) {
    if (!array->Get(i)->IsObject())
      continue;

    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(array->Get(i));
    v8::Local<v8::Object> keyBuffer =
      obj->Get(Nan::New("key").ToLocalChecked()).As<v8::Object>();
    v8::Local<v8::Value> type = obj->Get(Nan::New("type").ToLocalChecked());

    if (type->StrictEquals(Nan::New("del").ToLocalChecked())) {
      LD_STRING_OR_BUFFER_TO_SLICE(key, keyBuffer, key)

      batch->Delete(keyBuffer, key);
    } else if (type->StrictEquals(Nan::New("put").ToLocalChecked())) {
      v8::Local<v8::Object> valueBuffer =
        obj->Get(Nan::New("value").ToLocalChecked()).As<v8::Object>();

      LD_STRING_OR_BUFFER_TO_SLICE(key, keyBuffer, key)
      LD_STRING_OR_BUFFER_TO_SLICE(value, valueBuffer, value)
      batch->Put(keyBuffer, key, valueBuffer, value);
    }
  }

  batch->Write(callback);
}

NAN_METHOD(Database::ApproximateSize) {
  v8::Local<v8::Object> startBuffer = info[0].As<v8::Object>();
  v8::Local<v8::Object> endBuffer = info[1].As<v8::Object>();
  MDB_val* start = NULL;
  MDB_val* end = NULL;

  LD_METHOD_SETUP_COMMON(approximateSize, -1, 2)

  LD_STRING_OR_BUFFER_TO_COPY(start, startBuffer, start)
  LD_STRING_OR_BUFFER_TO_COPY(end, endBuffer, end)

  ApproximateSizeWorker* worker = new ApproximateSizeWorker(
      database
    , new Nan::Callback(callback)
    , start
    , end
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(Database::Backup) {
  v8::Local<v8::Object> pathBuffer = info[0].As<v8::Object>();
  Nan::Utf8String *path = new Nan::Utf8String(pathBuffer);

  LD_METHOD_SETUP_COMMON(backup, -1, 1)

  BackupWorker* worker = new BackupWorker(
      database
    , new Nan::Callback(callback)
    , path
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("database", _this);
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(Database::GetProperty) {
  v8::Local<v8::Value> propertyBuffer = info[0].As<v8::Object>();
  Nan::Utf8String property(propertyBuffer);

  leveldown::Database* database =
      Nan::ObjectWrap::Unwrap<leveldown::Database>(info.This());

  std::string value;
  database->GetPropertyFromDatabase(*property, &value);
  v8::Local<v8::String> returnValue
      = Nan::New<v8::String>(value.c_str(), value.length()).ToLocalChecked();

  info.GetReturnValue().Set(returnValue);
}

NAN_METHOD(Database::Iterator) {
  Database* database = Nan::ObjectWrap::Unwrap<Database>(info.This());

  v8::Local<v8::Object> optionsObj;
  if (info.Length() > 0 && info[0]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(info[0]);
  }

  // each iterator gets a unique id for this Database, so we can
  // easily store & lookup on our `iterators` map
  uint32_t id = database->currentIteratorId++;
  Nan::TryCatch try_catch;
  v8::Local<v8::Object> iteratorHandle = Iterator::NewInstance(
      info.This()
    , Nan::New<v8::Number>(id)
    , optionsObj
  );
  if (try_catch.HasCaught()) {
    // NB: node::FatalException can segfault here if there is no room on stack.
    return Nan::ThrowError("Fatal Error in Database::Iterator!");
  }

  leveldown::Iterator *iterator =
      Nan::ObjectWrap::Unwrap<leveldown::Iterator>(iteratorHandle);

  database->iterators[id] = iterator;

  // register our iterator
  /*
  v8::Local<v8::Object> obj = Nan::New<v8::Object>();
  obj->Set(Nan::New("iterator"), iteratorHandle);
  Nan::Persistent<v8::Object> persistent;
  persistent.Reset(nan_isolate, obj);
  database->iterators.insert(std::pair< uint32_t, Nan::Persistent<v8::Object> & >
      (id, persistent));
  */

  info.GetReturnValue().Set(iteratorHandle);
}

} // namespace leveldown
