/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include "database.h"
#include "database_async.h"
#include <stdlib.h>

namespace nlmdb {

Database::Database (const char* location) : location(location) {
  //dbi = NULL;
};

Database::~Database () {
  //if (dbi != NULL)
  //  delete dbi;
  delete[] location;
};

const char* Database::Location() const {
  return location;
}

int Database::OpenDatabase () {
  int env_opt = 0;
  env_opt = MDB_NOSYNC;
  env_opt |= MDB_NOMETASYNC;
  //env_opt |= MDB_WRITEMAP;
  int rc = mdb_env_create(&env);
  if (rc)
    return rc;
  //rc = mdb_env_set_mapsize(env, 3 * 1024 * 1024 * 1024l);
  if (rc)
    return rc;

  //TODO: yuk
  char cmd[200];
  sprintf(cmd, "mkdir -p %s", location);
  rc = system(cmd);
  if (rc)
    return rc;

  rc = mdb_env_open(env, location, env_opt, 0664);
  return rc;
}

int Database::PutToDatabase (MDB_val key, MDB_val value) {
  int rc;
  MDB_txn *txn;

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc)
    return rc;
  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc)
    return rc;
  rc = mdb_put(txn, dbi, &key, &value, 0);
  if (rc)
    return rc;
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
  if (rc)
    return rc;
  rc = mdb_get(txn, dbi, &key, &value);
  if (rc)
    return rc;
  rc = mdb_txn_commit(txn);
  return rc;
}

v8::Persistent<v8::Function> Database::constructor;

v8::Handle<v8::Value> NLMDB (const v8::Arguments& args) {
  v8::HandleScope scope;
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
  /*
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("del")
    , v8::FunctionTemplate::New(Delete)->GetFunction()
  );
  */

  constructor = v8::Persistent<v8::Function>::New(
      NL_NODE_ISOLATE_PRE
      tpl->GetFunction());
}

v8::Handle<v8::Value> Database::New (const v8::Arguments& args) {
  v8::HandleScope scope;

  if (args.Length() == 0) {
    NL_THROW_RETURN(constructor requires at least a location argument)
  }

  if (!args[0]->IsString()) {
    NL_THROW_RETURN(constructor requires a location string argument)
  }

  char* location = FromV8String(args[0]);

  Database* obj = new Database(location);
  obj->Wrap(args.This());

  return args.This();
}

v8::Handle<v8::Value> Database::NewInstance (const v8::Arguments& args) {
  v8::HandleScope scope;

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
  v8::HandleScope scope;

  NL_METHOD_SETUP_COMMON_ONEARG(open)

  OpenWorker* worker = new OpenWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
  );

  AsyncQueueWorker(worker);

  return v8::Undefined();
}

v8::Handle<v8::Value> Database::Close (const v8::Arguments& args) {
  v8::HandleScope scope;

  NL_METHOD_SETUP_COMMON_ONEARG(close)

  CloseWorker* worker = new CloseWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
  );

  AsyncQueueWorker(worker);

  return v8::Undefined();
}

v8::Handle<v8::Value> Database::Put (const v8::Arguments& args) {
  v8::HandleScope scope;

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

  WriteWorker* worker  = new WriteWorker(
      database
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , key
    , value
    , keyBuffer
    , valueBuffer
  );
  AsyncQueueWorker(worker);

  return v8::Undefined();
}

v8::Handle<v8::Value> Database::Get (const v8::Arguments& args) {
  v8::HandleScope scope;

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

  return v8::Undefined();
}

} // namespace nlmdb
