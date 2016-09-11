#include <node.h>
#include <node_buffer.h>
#include <nan.h>

#include "database.h"
#include "batch_async.h"
#include "batch.h"
#include "common.h"

namespace leveldown {

static Nan::Persistent<v8::FunctionTemplate> batch_constructor;

BatchOp::BatchOp (v8::Local<v8::Object> &keyHandle, MDB_val key) : key(key) {
  Nan::HandleScope scope;

  v8::Local<v8::Object> obj = Nan::New<v8::Object>();
  obj->Set(Nan::New("key").ToLocalChecked(), keyHandle);
  persistentHandle.Reset(obj);
}

BatchOp::~BatchOp () {
  Nan::HandleScope scope;

  v8::Local<v8::Object> handle = Nan::New<v8::Object>(persistentHandle);
  v8::Local<v8::Object> keyHandle =
      handle->Get(Nan::New("key").ToLocalChecked()).As<v8::Object>();
  DisposeStringOrBufferFromSlice(keyHandle, key);

  if (!persistentHandle.IsEmpty())
    persistentHandle.Reset();
}

BatchDel::BatchDel (v8::Local<v8::Object> &keyHandle, MDB_val key)
  : BatchOp(keyHandle, key) {}

BatchDel::~BatchDel () {}

int BatchDel::Execute (MDB_txn *txn, MDB_dbi dbi) {
  return mdb_del(txn, dbi, &key, NULL);
}

BatchPut::BatchPut (
    v8::Local<v8::Object> &keyHandle
  , MDB_val key
  , v8::Local<v8::Object> &valueHandle
  , MDB_val value
) : BatchOp(keyHandle, key)
  , value(value)
{
  Nan::HandleScope scope;
  v8::Local<v8::Object> handle = Nan::New<v8::Object>(persistentHandle);
  handle->Set(Nan::New("value").ToLocalChecked(), valueHandle);
}

BatchPut::~BatchPut () {
  Nan::HandleScope scope;

  v8::Local<v8::Object> handle = Nan::New<v8::Object>(persistentHandle);
  v8::Local<v8::Object> valueHandle =
      handle->Get(Nan::New("value").ToLocalChecked()).As<v8::Object>();

  DisposeStringOrBufferFromSlice(valueHandle, value);
}

int BatchPut::Execute (MDB_txn *txn, MDB_dbi dbi) {
  return mdb_put(txn, dbi, &key, &value, 0);
}

WriteBatch::WriteBatch (leveldown::Database* database, bool sync) : database(database) {
  operations = new std::vector<BatchOp*>;
  written = false;
}

WriteBatch::~WriteBatch () {
  Clear();
  delete operations;
}

void WriteBatch::Put (
      v8::Local<v8::Object> &keyHandle
    , MDB_val key
    , v8::Local<v8::Object> &valueHandle
    , MDB_val value) {
  operations->push_back(new BatchPut(keyHandle, key, valueHandle, value));
}

void WriteBatch::Delete (v8::Local<v8::Object> &keyHandle, MDB_val key) {
  operations->push_back(new BatchDel(keyHandle, key));
}

void WriteBatch::Clear () {
  for (std::vector< BatchOp* >::iterator it = operations->begin()
    ; it != operations->end()
    ; ) {
    delete *it;
    it = operations->erase(it);
  }
}

void WriteBatch::Write (v8::Local<v8::Function> callback) {
  Nan::HandleScope scope;

  written = true;

  if (operations->size() > 0) {
    BatchWriteWorker* worker = new BatchWriteWorker(
      this, new Nan::Callback(callback));
    Nan::AsyncQueueWorker(worker);
  } else {
    LD_RUN_CALLBACK(callback, 0, NULL);
  }
}

void WriteBatch::Init () {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(WriteBatch::New);
  batch_constructor.Reset(tpl);
  tpl->SetClassName(Nan::New("Batch").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetPrototypeMethod(tpl, "put", WriteBatch::Put);
  Nan::SetPrototypeMethod(tpl, "del", WriteBatch::Del);
  Nan::SetPrototypeMethod(tpl, "clear", WriteBatch::Clear);
  Nan::SetPrototypeMethod(tpl, "write", WriteBatch::Write);
}

NAN_METHOD(WriteBatch::New) {
  Database* database = Nan::ObjectWrap::Unwrap<Database>(info[0]->ToObject());
  v8::Local<v8::Object> optionsObj;

  if (info.Length() > 1 && info[1]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(info[1]);
  }

  bool sync = BooleanOptionValue(optionsObj, "sync");

  WriteBatch* batch = new WriteBatch(database, sync);
  batch->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

v8::Local<v8::Value> WriteBatch::NewInstance (
        v8::Local<v8::Object> database
      , v8::Local<v8::Object> optionsObj
    ) {

  Nan::EscapableHandleScope scope;

  Nan::MaybeLocal<v8::Object> maybeInstance;
  v8::Local<v8::Object> instance;

  v8::Local<v8::FunctionTemplate> constructorHandle =
      Nan::New<v8::FunctionTemplate>(batch_constructor);

  if (optionsObj.IsEmpty()) {
    v8::Local<v8::Value> argv[1] = { database };
    maybeInstance = Nan::NewInstance(constructorHandle->GetFunction(), 1, argv);
  } else {
    v8::Local<v8::Value> argv[2] = { database, optionsObj };
    maybeInstance = Nan::NewInstance(constructorHandle->GetFunction(), 2, argv);
  }

  if (maybeInstance.IsEmpty())
    Nan::ThrowError("Could not create new Batch instance");
  else
    instance = maybeInstance.ToLocalChecked();

  return scope.Escape(instance);
}

NAN_METHOD(WriteBatch::Put) {
  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(info.Holder());

  if (batch->written) {
    return Nan::ThrowError("write() already called on this batch");
  }

  v8::Local<v8::Function> callback; // purely for the error macros

  LD_CB_ERR_IF_NULL_OR_UNDEFINED(info[0], key)
  LD_CB_ERR_IF_NULL_OR_UNDEFINED(info[1], value)

  v8::Local<v8::Object> keyBuffer = info[0].As<v8::Object>();
  v8::Local<v8::Object> valueBuffer = info[1].As<v8::Object>();
  LD_STRING_OR_BUFFER_TO_SLICE(key, keyBuffer, key)
  LD_STRING_OR_BUFFER_TO_SLICE(value, valueBuffer, value)

  batch->Put(keyBuffer, key, valueBuffer, value);

  info.GetReturnValue().Set(info.Holder());
}

NAN_METHOD(WriteBatch::Del) {
  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(info.Holder());

  v8::Local<v8::Function> callback; // purely for the error macros

  LD_CB_ERR_IF_NULL_OR_UNDEFINED(info[0], key)

  v8::Local<v8::Object> keyBuffer = info[0].As<v8::Object>();
  LD_STRING_OR_BUFFER_TO_SLICE(key, keyBuffer, key)

  batch->Delete(keyBuffer, key);

  info.GetReturnValue().Set(info.Holder());
}

NAN_METHOD(WriteBatch::Clear) {
  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(info.Holder());

  if (batch->written) {
    return Nan::ThrowError("write() already called on this batch");
  }

  batch->Clear();

  info.GetReturnValue().Set(info.Holder());
}

NAN_METHOD(WriteBatch::Write) {
  WriteBatch* batch = ObjectWrap::Unwrap<WriteBatch>(info.Holder());

  if (info.Length() == 0 || !info[0]->IsFunction()) {
    return Nan::ThrowError("write() requires a callback argument");
  }

  if (batch->written) {
    return Nan::ThrowError("write() already called on this batch");
  }

  batch->written = true;

  if (batch->operations->size() > 0) {
    Nan::Callback *callback =
        new Nan::Callback(v8::Local<v8::Function>::Cast(info[0]));
    BatchWriteWorker* worker = new BatchWriteWorker(batch, callback);
    // persist to prevent accidental GC
    v8::Local<v8::Object> _this = info.This();
    worker->SaveToPersistent("batch", _this);
    Nan::AsyncQueueWorker(worker);
  } else {
    LD_RUN_CALLBACK(v8::Local<v8::Function>::Cast(info[0]), 0, NULL);
  }
}

} // namespace leveldown
