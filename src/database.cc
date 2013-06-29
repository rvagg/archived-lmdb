/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>
#include <sys/stat.h>
#include <stdlib.h> // TODO: remove when we remove system()

#include "database.h"
#include "database_async.h"
#include "batch.h"
#include "iterator.h"

#include <iostream> 
#include <string.h>

namespace nlmdb {

inline const uv_statbuf_t* Stat (const char* path) {
  uv_fs_t req;
  int result = uv_fs_lstat(uv_default_loop(), &req, path, NULL);
  if (result < 0)
    return NULL;
  return static_cast<const uv_statbuf_t*>(req.ptr);
}

inline bool IsDirectory (const uv_statbuf_t* stat) {
  return (stat->st_mode & S_IFMT) == S_IFDIR;
}

inline bool MakeDirectory (const char* path) {
  uv_fs_t req;
  return uv_fs_mkdir(uv_default_loop(), &req, path, 511, NULL);
}

Database::Database (const char* location) : location(location) {
  //dbi = NULL;
  currentIteratorId = 0;
  pendingCloseWorker = NULL;
};

Database::~Database () {
  //if (dbi != NULL)
  //  delete dbi;
  delete[] location;
};

const char* Database::Location() const {
  return location;
}

void Database::ReleaseIterator (uint32_t id) {
  // called each time an Iterator is End()ed, in the main thread
  // we have to remove our reference to it and if it's the last iterator
  // we have to invoke a pending CloseWorker if there is one
  // if there is a pending CloseWorker it means that we're waiting for
  // iterators to end before we can close them
  iterators.erase(id);
  //std::cerr << "ReleaseIterator: " << iterators.size() << std::endl;
  if (iterators.size() == 0 && pendingCloseWorker != NULL) {
    //std::cerr << "pendingCloseWorker RUNNING\n";
    AsyncQueueWorker((AsyncWorker*)pendingCloseWorker);
    pendingCloseWorker = NULL;
  }
}

md_status Database::OpenDatabase (OpenOptions options) {
  md_status status;

  // Emulate the behaviour of LevelDB create_if_missing & error_if_exists
  // options, with an additional check for stat == directory
  const uv_statbuf_t* stat = Stat(location);
  if (stat == NULL) {
    if (options.createIfMissing) {
      status.code = MakeDirectory(location);
      if (status.code)
        return status;
    } else {
      status.error = std::string(location);
      status.error += " does not exist (createIfMissing is false)";
      return status;
    }
  } else {
    if (!IsDirectory(stat)) {
      status.error = std::string(location);
      status.error += " exists and is not a directory";
      return status;
    }
    if (options.errorIfExists) {
      status.error = std::string(location);
      status.error += " exists (errorIfExists is true)";
      return status;
    }
  }

  int env_opt = 0;
  env_opt = MDB_NOSYNC;
  env_opt |= MDB_NOMETASYNC;
  //env_opt |= MDB_WRITEMAP;
  status.code = mdb_env_create(&env);
  if (status.code)
    return status;

  status.code = mdb_env_set_mapsize(env, options.mapSize);
  if (status.code)
    return status;

  //TODO: yuk
  if (options.createIfMissing) {
    char cmd[200];
    sprintf(cmd, "mkdir -p %s", location);
    status.code = system(cmd);
    if (status.code)
      return status;
  }

  status.code = mdb_env_open(env, location, env_opt, 0664);
  return status;
}

int Database::PutToDatabase (MDB_val key, MDB_val value) {
  int rc;
  MDB_txn *txn;

  //std::cerr << "PUTTODB(" << (char*)key.mv_data << "(" << key.mv_size << ")," << (char*)value.mv_data << "(" << value.mv_size << "))" << std::endl;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;
  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }
  rc = mdb_put(txn, dbi, &key, &value, 0);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }
  rc = mdb_txn_commit(txn);
  //std::cerr << "FINISHED PUTTODB: " << rc << std::endl;
  return rc;
}

int Database::PutToDatabase (std::vector< BatchOp* >* operations) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;
  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }

  for (std::vector< BatchOp* >::iterator it = operations->begin()
      ; it != operations->end()
      ; it++) {

    rc = (*it)->Execute(txn, dbi);
    if (rc) {
      mdb_txn_abort(txn);
      return rc;
    }
  }

  rc = mdb_txn_commit(txn);

  return rc;
}

int Database::GetFromDatabase (MDB_val key, MDB_val& value) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;
  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }
  rc = mdb_get(txn, dbi, &key, &value);
  if (rc) {
    mdb_txn_abort(txn);
    return rc;
  }
  rc = mdb_txn_commit(txn);
  return rc;
}

int Database::DeleteFromDatabase (MDB_val key) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;
  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc)
    return rc;
  rc = mdb_del(txn, dbi, &key, NULL);
  if (rc)
    return rc;
  rc = mdb_txn_commit(txn);
  return rc;
}

int Database::NewIterator (MDB_txn **txn, MDB_cursor **cursor) {
  int rc;

  rc = mdb_txn_begin(env, NULL, 0, txn);
  //std::cerr << "opened transaction! " << cursor << ", " << strerror(rc) << std::endl;
  if (rc)
    return rc;
  rc = mdb_open(*txn, NULL, 0, &dbi);
  if (rc) {
    mdb_txn_abort(*txn);
    return rc;
  }
  rc = mdb_cursor_open(*txn, dbi, cursor);
  //std::cerr << "opened cursor! " << cursor << ", " << strerror(rc) << std::endl;
  return rc;
}

v8::Persistent<v8::Function> Database::constructor;

v8::Handle<v8::Value> NLMDB (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  return scope.Close(Database::NewInstance(args));
}

void Database::Init () {
  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
  tpl->SetClassName(v8::String::NewSymbol("Database"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("open")
    , v8::FunctionTemplate::New(Open)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("close")
    , v8::FunctionTemplate::New(Close)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("put")
    , v8::FunctionTemplate::New(Put)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("get")
    , v8::FunctionTemplate::New(Get)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("del")
    , v8::FunctionTemplate::New(Delete)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("batch")
    , v8::FunctionTemplate::New(Batch)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("iterator")
    , v8::FunctionTemplate::New(Iterator)->GetFunction()
  );

  constructor = v8::Persistent<v8::Function>::New(
      NL_NODE_ISOLATE_PRE
      tpl->GetFunction());
}

v8::Handle<v8::Value> Database::New (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  if (args.Length() == 0) {
    NL_THROW_RETURN(constructor requires at least a location argument)
  }

  if (!args[0]->IsString()) {
    NL_THROW_RETURN(constructor requires a location string argument)
  }

  char* location = FromV8String(args[0]);

  Database* obj = new Database(location);
  obj->Wrap(args.This());

  return scope.Close(args.This());
}

v8::Handle<v8::Value> Database::NewInstance (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  v8::Local<v8::Object> instance;

  if (args.Length() == 0) {
    instance = constructor->NewInstance(0, NULL);
  } else {
    v8::Handle<v8::Value> argv[] = { args[0] };
    instance = constructor->NewInstance(1, argv);
  }

  return scope.Close(instance);
}

v8::Handle<v8::Value> Database::Open (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  NL_METHOD_SETUP_COMMON(open, 0, 1)

  OpenOptions options;

  options.createIfMissing = BooleanOptionValueDefTrue(
      optionsObj
    , option_createIfMissing
  );
  options.errorIfExists = BooleanOptionValue(optionsObj, option_errorIfExists);
  options.mapSize = UInt32OptionValue(
      optionsObj
    , option_mapSize
    , DEFAULT_MAPSIZE
  );

  OpenWorker* worker = new OpenWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , options
  );

  AsyncQueueWorker(worker);

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> Database::Close (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  NL_METHOD_SETUP_COMMON_ONEARG(close)

  CloseWorker* worker = new CloseWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
  );

  if (database->iterators.size() > 0) {
    // yikes, we still have iterators open! naughty naughty.
    // we have to queue up a CloseWorker and manually close each of them.
    // the CloseWorker will be invoked once they are all cleaned up
    database->pendingCloseWorker = worker;

  //std::cerr << "--------------------------------------------------------\n";
    for (
        std::map< uint32_t, v8::Persistent<v8::Object> >::iterator it
            = database->iterators.begin()
      ; it != database->iterators.end()
      ; ++it) {
  //std::cerr << "Releasing iterator...\n";

        // for each iterator still open, first check if it's already in
        // the process of ending (ended==true means an async End() is
        // in progress), if not, then we call End() with an empty callback
        // function and wait for it to hit ReleaseIterator() where our
        // CloseWorker will be invoked

        nlmdb::Iterator* iterator
          = node::ObjectWrap::Unwrap<nlmdb::Iterator>(it->second);

        if (!iterator->ended) {
          v8::Local<v8::Function> end =
              v8::Local<v8::Function>::Cast(it->second->Get(
                  v8::String::NewSymbol("end")));
          v8::Local<v8::Value> argv[] = {
              v8::FunctionTemplate::New()->GetFunction() // empty callback
          };
          v8::TryCatch try_catch;
          end->Call(it->second, 1, argv);
          if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
          }
        }
    }
  //std::cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  } else {
    AsyncQueueWorker(worker);
  }

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> Database::Put (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  NL_METHOD_SETUP_COMMON(put, 2, 3)

  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[0], key)
  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[1], value)

  v8::Local<v8::Value> keyBufferV = args[0];
  v8::Local<v8::Value> valueBufferV = args[1];
  NL_STRING_OR_BUFFER_TO_MDVAL(key, keyBufferV, key)
  NL_STRING_OR_BUFFER_TO_MDVAL(value, valueBufferV, value)

  v8::Persistent<v8::Value> keyBuffer =
      v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE keyBufferV);
  v8::Persistent<v8::Value> valueBuffer =
      v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE valueBufferV);

  //std::cerr << "PUT(" << (char*)key.mv_data << "(" << key.mv_size << ")," << (char*)value.mv_data << "(" << value.mv_size << "))" << std::endl;

  WriteWorker* worker  = new WriteWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , key
    , value
    , keyBuffer
    , valueBuffer
  );
  AsyncQueueWorker(worker);

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> Database::Get (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  NL_METHOD_SETUP_COMMON(get, 1, 2)

  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[0], key)

  v8::Local<v8::Value> keyBufferV = args[0];
  NL_STRING_OR_BUFFER_TO_MDVAL(key, keyBufferV, key)

  v8::Persistent<v8::Value> keyBuffer = v8::Persistent<v8::Value>::New(
      NL_NODE_ISOLATE_PRE
      keyBufferV);

  bool asBuffer = BooleanOptionValueDefTrue(optionsObj, option_asBuffer);

  ReadWorker* worker = new ReadWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , key
    , asBuffer
    , keyBuffer
  );
  AsyncQueueWorker(worker);

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> Database::Delete (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  NL_METHOD_SETUP_COMMON(del, 1, 2)

  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[0], key)

  v8::Local<v8::Value> keyBufferV = args[0];
  NL_STRING_OR_BUFFER_TO_MDVAL(key, keyBufferV, key)

  v8::Persistent<v8::Value> keyBuffer = v8::Persistent<v8::Value>::New(
      NL_NODE_ISOLATE_PRE
      keyBufferV);

  DeleteWorker* worker = new DeleteWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , key
    , keyBuffer
  );
  AsyncQueueWorker(worker);

  return scope.Close(v8::Undefined());
}

/* property key & value strings for elements of the array sent to batch() */
NL_SYMBOL ( str_key   , key   );
NL_SYMBOL ( str_value , value );
NL_SYMBOL ( str_type  , type  );
NL_SYMBOL ( str_del   , del   );
NL_SYMBOL ( str_put   , put   );

v8::Handle<v8::Value> Database::Batch (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  if ((args.Length() == 0 || args.Length() == 1) && !args[0]->IsArray()) {
    v8::Local<v8::Object> optionsObj;
    if (args.Length() > 0 && args[0]->IsObject()) {
      optionsObj = v8::Local<v8::Object>::Cast(args[0]);
    }
    return scope.Close(WriteBatch::NewInstance(args.This(), optionsObj));
  }

  NL_METHOD_SETUP_COMMON(batch, 1, 2)

  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(args[0]);
  WriteBatch* batch = new WriteBatch(database);

  for (unsigned int i = 0; i < array->Length(); i++) {
    if (!array->Get(i)->IsObject())
      continue;

    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(array->Get(i));

    NL_CB_ERR_IF_NULL_OR_UNDEFINED(obj->Get(str_type), type)

    v8::Local<v8::Value> keyBuffer = obj->Get(str_key);
    NL_CB_ERR_IF_NULL_OR_UNDEFINED(keyBuffer, key)

    if (obj->Get(str_type)->StrictEquals(str_del)) {
      NL_STRING_OR_BUFFER_TO_MDVAL(key, keyBuffer, key)
      batch->Delete(v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE keyBuffer), key);
    } else if (obj->Get(str_type)->StrictEquals(str_put)) {
      v8::Local<v8::Value> valueBuffer = obj->Get(str_value);
      NL_CB_ERR_IF_NULL_OR_UNDEFINED(valueBuffer, value)
      NL_STRING_OR_BUFFER_TO_MDVAL(key, keyBuffer, key)
      NL_STRING_OR_BUFFER_TO_MDVAL(value, valueBuffer, value)

      batch->Put(
          v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE keyBuffer)
        , key
        , v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE valueBuffer)
        , value
      );
    }
  }

  batch->Write(callback);

  return v8::Undefined();
}

v8::Handle<v8::Value> Database::Iterator (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  Database* database = node::ObjectWrap::Unwrap<Database>(args.This());

  v8::Local<v8::Object> optionsObj;
  if (args.Length() > 0 && args[0]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(args[0]);
  }

  // each iterator gets a unique id for this Database, so we can
  // easily store & lookup on our `iterators` map
  uint32_t id = database->currentIteratorId++;
  v8::TryCatch try_catch;
  v8::Handle<v8::Object> iterator = Iterator::NewInstance(
      args.This()
    , v8::Number::New(id)
    , optionsObj
  );
  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }

  // register our iterator
  database->iterators[id] =
      v8::Persistent<v8::Object>::New(
          node::ObjectWrap::Unwrap<nlmdb::Iterator>(iterator)->handle_);

  return scope.Close(iterator);
}

} // namespace nlmdb
