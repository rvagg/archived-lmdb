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
  , OpenOptions options
) : AsyncWorker(database, callback)
  , options(options)
{ };

OpenWorker::~OpenWorker () { }

void OpenWorker::Execute () {
  status = database->OpenDatabase(options);
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
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

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
  status.code = database->GetFromDatabase(key, value);
}

void ReadWorker::HandleOKCallback () {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

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
  status.code = database->DeleteFromDatabase(key);
}

void DeleteWorker::WorkComplete () {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  if (status.code == MDB_NOTFOUND || (status.code == 0 && status.error.length() == 0))
    HandleOKCallback();
  else
    HandleErrorCallback();

  callback.Dispose(NL_NODE_ISOLATE);
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
  status.code = database->PutToDatabase(key, value);
}

void WriteWorker::WorkComplete () {
  DisposeStringOrBufferFromMDVal(valuePtr, value);
  IOWorker::WorkComplete();
}

} // namespace nlmdb
