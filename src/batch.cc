/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>
#include <iostream>
#include "nlmdb.h"
#include "database.h"
#include "batch_async.h"
#include "batch.h"

namespace nlmdb {

BatchOp::BatchOp (v8::Persistent<v8::Value> keyPtr, MDB_val key)
  : keyPtr(keyPtr), key(key) {}

BatchOp::~BatchOp () {
  DisposeStringOrBufferFromMDVal(keyPtr, key);
}

BatchDel::BatchDel (v8::Persistent<v8::Value> keyPtr, MDB_val key)
  : BatchOp(keyPtr, key) {}

BatchDel::~BatchDel () {}

int BatchDel::Execute (MDB_txn *txn, MDB_dbi dbi) {
  return mdb_del(txn, dbi, &key, 0);
}

BatchPut::BatchPut (
    v8::Persistent<v8::Value> keyPtr
  , MDB_val key
  , v8::Persistent<v8::Value> valuePtr
  , MDB_val value
) : BatchOp(keyPtr, key)
  , valuePtr(valuePtr)
  , value(value)
{}

BatchPut::~BatchPut () {
  DisposeStringOrBufferFromMDVal(valuePtr, value);
}

int BatchPut::Execute (MDB_txn *txn, MDB_dbi dbi) {
  return mdb_put(txn, dbi, &key, &value, 0);
}

v8::Persistent<v8::Function> WriteBatch::constructor;

WriteBatch::WriteBatch (Database* database) : database(database) {
  operations = new std::vector<BatchOp*>;
  written = false;
}

WriteBatch::~WriteBatch () {
  std::cerr << "~WriteBatch()\n";
  Clear();
  delete operations;
}

void WriteBatch::Write (v8::Local<v8::Function> callback) {
  NL_NODE_ISOLATE_DECL

  written = true;

  if (operations->size() > 0) {
    AsyncQueueWorker(new BatchWriteWorker(
        this
      , v8::Persistent<v8::Function>::New(
            NL_NODE_ISOLATE_PRE
            callback
        )
    ));
  } else {
    NL_RUN_CALLBACK(callback, NULL, 0);
  }
}

void WriteBatch::Put (
      v8::Persistent<v8::Value> keyPtr
    , MDB_val key
    , v8::Persistent<v8::Value> valuePtr
    , MDB_val value) {

  operations->push_back(new BatchPut(keyPtr, key, valuePtr, value));
}

void WriteBatch::Delete (v8::Persistent<v8::Value> keyPtr, MDB_val key) {
  operations->push_back(new BatchDel(keyPtr, key));
}

void WriteBatch::Clear () {
    for (std::vector< BatchOp* >::iterator it = operations->begin()
      ; it != operations->end()
      ; ) {

    delete *it;
    it = operations->erase(it);
  }
}

void WriteBatch::Init () {
  NL_NODE_ISOLATE_DECL

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(WriteBatch::New);
  tpl->SetClassName(v8::String::NewSymbol("Batch"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("put")
    , v8::FunctionTemplate::New(WriteBatch::Put)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("del")
    , v8::FunctionTemplate::New(WriteBatch::Del)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("clear")
    , v8::FunctionTemplate::New(WriteBatch::Clear)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("write")
    , v8::FunctionTemplate::New(WriteBatch::Write)->GetFunction()
  );
  constructor = v8::Persistent<v8::Function>::New(
      NL_NODE_ISOLATE_PRE
      tpl->GetFunction());
}

v8::Handle<v8::Value> WriteBatch::New (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  Database* database = node::ObjectWrap::Unwrap<Database>(args[0]->ToObject());
  v8::Local<v8::Object> optionsObj;

  if (args.Length() > 1 && args[1]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(args[1]);
  }

  WriteBatch* batch = new WriteBatch(database);
  batch->Wrap(args.This());

  return args.This();
}

v8::Handle<v8::Value> WriteBatch::NewInstance (
        v8::Handle<v8::Object> database
      , v8::Handle<v8::Object> optionsObj
    ) {

  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  v8::Local<v8::Object> instance;

  if (optionsObj.IsEmpty()) {
    v8::Handle<v8::Value> argv[1] = { database };
    instance = constructor->NewInstance(1, argv);
  } else {
    v8::Handle<v8::Value> argv[2] = { database, optionsObj };
    instance = constructor->NewInstance(2, argv);
  }

  return scope.Close(instance);
}

v8::Handle<v8::Value> WriteBatch::Put (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(args.Holder());

  if (batch->written) {
    NL_THROW_RETURN(write() already called on this batch)
  }

  v8::Handle<v8::Function> callback; // purely for the error macros

  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[0], key)
  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[1], value)

  v8::Local<v8::Value> keyPtr = args[0];
  v8::Local<v8::Value> valuePtr = args[1];
  NL_STRING_OR_BUFFER_TO_MDVAL(key, keyPtr, key)
  NL_STRING_OR_BUFFER_TO_MDVAL(value, valuePtr, value)

  batch->Put(
      v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE keyPtr)
    , key
    , v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE valuePtr)
    , value
  );

  return scope.Close(args.Holder());
}

v8::Handle<v8::Value> WriteBatch::Del (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(args.Holder());

  if (batch->written) {
    NL_THROW_RETURN(write() already called on this batch)
  }

  v8::Handle<v8::Function> callback; // purely for the error macros

  NL_CB_ERR_IF_NULL_OR_UNDEFINED(args[0], key)

  v8::Local<v8::Value> keyPtr = args[0];
  NL_STRING_OR_BUFFER_TO_MDVAL(key, keyPtr, key)

  batch->Delete(
      v8::Persistent<v8::Value>::New(NL_NODE_ISOLATE_PRE keyPtr)
    , key
  );
  return scope.Close(args.Holder());
}

v8::Handle<v8::Value> WriteBatch::Clear (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(args.Holder());

  if (batch->written) {
    NL_THROW_RETURN(write() already called on this batch)
  }

  batch->Clear();
  return scope.Close(args.Holder());
}

v8::Handle<v8::Value> WriteBatch::Write (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(args.Holder());

  if (batch->written) {
    NL_THROW_RETURN(write() already called on this batch)
  }
  
  if (args.Length() == 0) {
    NL_THROW_RETURN(write() requires a callback argument)
  }

  batch->Write(v8::Local<v8::Function>::Cast(args[0]));

  return v8::Undefined();
}

} // namespace nlmdb
