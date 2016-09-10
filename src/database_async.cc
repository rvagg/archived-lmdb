/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#include <node.h>
#include <node_buffer.h>

#include "database.h"
#include "leveldown.h"
#include "async.h"
#include "database_async.h"

namespace leveldown {

/** OPEN WORKER **/

OpenWorker::OpenWorker (
    Database* database
  , Nan::Callback *callback
  , OpenOptions options
) : AsyncWorker(database, callback)
  , options(options)
{ };

OpenWorker::~OpenWorker () { }

void OpenWorker::Execute () {
  SetStatus(database->OpenDatabase(options));
}

/** CLOSE WORKER **/

CloseWorker::CloseWorker (
    Database *database
  , Nan::Callback *callback
) : AsyncWorker(database, callback)
{};

CloseWorker::~CloseWorker () {}

void CloseWorker::Execute () {
  database->CloseDatabase();
}

void CloseWorker::WorkComplete () {
  Nan::HandleScope scope;
  HandleOKCallback();
  delete callback;
  callback = NULL;
}

/** IO WORKER (abstract) **/

IOWorker::IOWorker (
    Database *database
  , Nan::Callback *callback
  , MDB_val key
  , v8::Local<v8::Object> &keyHandle
) : AsyncWorker(database, callback)
  , key(key)
  , keyHandle(keyHandle)
{
  Nan::HandleScope scope;

  SaveToPersistent("key", keyHandle);
};

IOWorker::~IOWorker () {}

void IOWorker::WorkComplete () {
  Nan::HandleScope scope;

  DisposeStringOrBufferFromSlice(GetFromPersistent("key"), key);
  AsyncWorker::WorkComplete();
}

/** READ WORKER **/

ReadWorker::ReadWorker (
    Database *database
  , Nan::Callback *callback
  , MDB_val key
  , bool asBuffer
  , bool fillCache
  , v8::Local<v8::Object> &keyHandle
) : IOWorker(database, callback, key, keyHandle)
  , asBuffer(asBuffer)
{
  Nan::HandleScope scope;

  SaveToPersistent("key", keyHandle);
};

ReadWorker::~ReadWorker () { }

void ReadWorker::Execute () {
  SetStatus(database->GetFromDatabase(key, value));
}

void ReadWorker::HandleOKCallback () {
  Nan::HandleScope scope;

  v8::Local<v8::Value> returnValue;
  if (asBuffer) {
    //TODO: could use NewBuffer if we carefully manage the lifecycle of `value`
    //and avoid an an extra allocation. We'd have to clean up properly when not OK
    //and let the new Buffer manage the data when OK
    returnValue = Nan::CopyBuffer((char*)value.mv_data, value.mv_size).ToLocalChecked();
  } else {
    returnValue = Nan::New<v8::String>((char*)value.mv_data, value.mv_size).ToLocalChecked();
  }

  v8::Local<v8::Value> argv[] = {
      Nan::Null()
    , returnValue
  };

  callback->Call(2, argv);
}

/** DELETE WORKER **/

DeleteWorker::DeleteWorker (
    Database *database
  , Nan::Callback *callback
  , MDB_val key
  , bool sync
  , v8::Local<v8::Object> &keyHandle
) : IOWorker(database, callback, key, keyHandle)
{
  Nan::HandleScope scope;

  SaveToPersistent("key", keyHandle);
};

DeleteWorker::~DeleteWorker () { }

void DeleteWorker::Execute () {
  SetStatus(database->DeleteFromDatabase(key));
}

void DeleteWorker::WorkComplete () {
  Nan::HandleScope scope;

  if (status.code == MDB_NOTFOUND || (status.code == 0 && status.error.length() == 0))
    HandleOKCallback();
  else
    HandleErrorCallback();

  // IOWorker does this but we can't call IOWorker::WorkComplete()
  DisposeStringOrBufferFromSlice(GetFromPersistent("key"), key);
}

/** WRITE WORKER **/

WriteWorker::WriteWorker (
    Database *database
  , Nan::Callback *callback
  , MDB_val key
  , MDB_val value
  , bool sync
  , v8::Local<v8::Object> &keyHandle
  , v8::Local<v8::Object> &valueHandle
) : DeleteWorker(database, callback, key, sync, keyHandle)
  , value(value)
  , valueHandle(valueHandle)
{
  Nan::HandleScope scope;

  SaveToPersistent("value", valueHandle);
};

WriteWorker::~WriteWorker () { }

void WriteWorker::Execute () {
  SetStatus(database->PutToDatabase(key, value));
}

void WriteWorker::WorkComplete () {
  Nan::HandleScope scope;

  DisposeStringOrBufferFromSlice(GetFromPersistent("value"), value);
  IOWorker::WorkComplete();
}

/** APPROXIMATE SIZE WORKER **/

ApproximateSizeWorker::ApproximateSizeWorker (
    Database *database
  , Nan::Callback *callback
  , std::string* start
  , std::string* end
) : AsyncWorker(database, callback)
  , start(start)
  , end(end)
{ };

ApproximateSizeWorker::~ApproximateSizeWorker () {
  delete start;
  delete end;
}

void ApproximateSizeWorker::Execute () {
  size = database->ApproximateSizeFromDatabase(start, end);
}

void ApproximateSizeWorker::HandleOKCallback () {
  Nan::HandleScope scope;

  v8::Local<v8::Value> returnValue = Nan::New<v8::Number>((double) size);
  v8::Local<v8::Value> argv[] = {
      Nan::Null()
    , returnValue
  };
  callback->Call(2, argv);
}

/** BACKUP WORKER **/

BackupWorker::BackupWorker (
    Database *database
  , Nan::Callback *callback
  , Nan::Utf8String* path
) : AsyncWorker(database, callback)
  , path(path)
{ };

BackupWorker::~BackupWorker () {
  delete path;
}

void BackupWorker::Execute () {
  SetStatus(database->BackupDatabase(**path));
}

void BackupWorker::HandleOKCallback () {
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[] = {
    Nan::Null()
  };
  callback->Call(1, argv);
}

} // namespace leveldown
