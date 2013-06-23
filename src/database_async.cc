/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>
#include <node_buffer.h>

#include "database.h"
#include "nlmdb.h"
#include "async.h"
#include "database_async.h"

namespace nlmdb {

/** OPEN WORKER **/

OpenWorker::OpenWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
) : AsyncWorker(database, callback)
{ };

OpenWorker::~OpenWorker () { }

void OpenWorker::Execute () {
  status = database->OpenDatabase();
}

/** CLOSE WORKER **/

CloseWorker::CloseWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
) : AsyncWorker(database, callback)
{};

CloseWorker::~CloseWorker () {}

void CloseWorker::Execute () {
//  database->CloseDatabase();
}

void CloseWorker::WorkComplete () {
  v8::HandleScope scope;
  HandleOKCallback();
  callback.Dispose(NL_NODE_ISOLATE);
}

/** IO WORKER (abstract) **/
IOWorker::IOWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , MDB_val key
  , v8::Persistent<v8::Value> keyPtr
) : AsyncWorker(database, callback)
  , key(key)
  , keyPtr(keyPtr)
{};

IOWorker::~IOWorker () {}

void IOWorker::WorkComplete () {
  DisposeStringOrBufferFromMDVal(keyPtr, key);
  AsyncWorker::WorkComplete();
}

/** READ WORKER **/
ReadWorker::ReadWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , MDB_val key
  , bool asBuffer
  , v8::Persistent<v8::Value> keyPtr
) : IOWorker(database, callback, key, keyPtr)
  , asBuffer(asBuffer)
{};

ReadWorker::~ReadWorker () {}

void ReadWorker::Execute () {
  status = database->GetFromDatabase(key, value);
}

void ReadWorker::HandleOKCallback () {
  v8::HandleScope scope;
  v8::Local<v8::Value> returnValue;
  if (asBuffer)
    returnValue = v8::Local<v8::Value>::New(
      node::Buffer::New((char*)value.mv_data, value.mv_size)->handle_
    );
  else
    returnValue = v8::String::New((char*)value.mv_data, value.mv_size);
  v8::Local<v8::Value> argv[] = {
      v8::Local<v8::Value>::New(v8::Null())
    , returnValue
  };
  NL_RUN_CALLBACK(callback, argv, 2);
}

/** DELETE WORKER **/
DeleteWorker::DeleteWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , MDB_val key
  , v8::Persistent<v8::Value> keyPtr
) : IOWorker(database, callback, key, keyPtr)
{};

DeleteWorker::~DeleteWorker () {}

void DeleteWorker::Execute () {
  //TODO: status = database->DeleteFromDatabase(options, key);
}

/** WRITE WORKER **/
WriteWorker::WriteWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , MDB_val key
  , MDB_val value
  , v8::Persistent<v8::Value> keyPtr
  , v8::Persistent<v8::Value> valuePtr
) : DeleteWorker(database, callback, key, keyPtr)
  , value(value)
  , valuePtr(valuePtr)
{};

WriteWorker::~WriteWorker () {}

void WriteWorker::Execute () {
  status = database->PutToDatabase(key, value);
}

void WriteWorker::WorkComplete () {
  DisposeStringOrBufferFromMDVal(valuePtr, value);
  IOWorker::WorkComplete();
}

/** BATCH WORKER **/
/*
BatchWorker::BatchWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , leveldb::WriteBatch* batch
  , std::vector<Reference>* references
  , bool sync
) : AsyncWorker(database, callback)
  , batch(batch)
  , references(references)
{
  options = new leveldb::WriteOptions();
  options->sync = sync;
};

BatchWorker::~BatchWorker () {
  ClearReferences(references);
  delete options;
}

void BatchWorker::Execute () {
  status = database->WriteBatchToDatabase(options, batch);
}

/** APPROXIMATE SIZE WORKER **/
/*
ApproximateSizeWorker::ApproximateSizeWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
  , MDB_val start
  , MDB_val end
  , v8::Persistent<v8::Value> startPtr
  , v8::Persistent<v8::Value> endPtr
) : AsyncWorker(database, callback)
  , range(start, end)
  , startPtr(startPtr)
  , endPtr(endPtr)
{};

ApproximateSizeWorker::~ApproximateSizeWorker () {}

void ApproximateSizeWorker::Execute () {
  size = database->ApproximateSizeFromDatabase(&range);
}

void ApproximateSizeWorker::WorkComplete() {
  DisposeStringOrBufferFromMDVal(startPtr, range.start);
  DisposeStringOrBufferFromMDVal(endPtr, range.limit);
  AsyncWorker::WorkComplete();
}

void ApproximateSizeWorker::HandleOKCallback () {
  v8::HandleScope scope;
  v8::Local<v8::Value> returnValue = v8::Number::New((double) size);
  v8::Local<v8::Value> argv[] = {
      v8::Local<v8::Value>::New(v8::Null())
    , returnValue
  };
  NL_RUN_CALLBACK(callback, argv, 2);
}
*/

} // namespace nlmdb
