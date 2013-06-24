/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>

#include "database.h"
#include "nlmdb.h"
#include "async.h"

namespace nlmdb {

/** ASYNC BASE **/

AsyncWorker::AsyncWorker (
    Database* database
  , v8::Persistent<v8::Function> callback
) : database(database)
  , callback(callback)
{
    request.data = this;
    status.code = 0;
};

AsyncWorker::~AsyncWorker () {}

void AsyncWorker::WorkComplete () {
  v8::HandleScope scope;
  if (status.code == 0 && status.error.length() == 0)
    HandleOKCallback();
  else
    HandleErrorCallback();
  callback.Dispose(NL_NODE_ISOLATE);
}

void AsyncWorker::HandleOKCallback () {
  NL_RUN_CALLBACK(callback, NULL, 0);  
}

void AsyncWorker::HandleErrorCallback () {
  v8::HandleScope scope;
  const char* err;
  if (status.error.length() != 0)
    err = status.error.c_str();
  else
    err = mdb_strerror(status.code);

  v8::Local<v8::Value> argv[] = {
      v8::Local<v8::Value>::New(
        v8::Exception::Error(v8::String::New(err))
      )
  };
  NL_RUN_CALLBACK(callback, argv, 1);
}

void AsyncExecute (uv_work_t* req) {
  static_cast<AsyncWorker*>(req->data)->Execute();
}

void AsyncExecuteComplete (uv_work_t* req) {
  AsyncWorker* worker = static_cast<AsyncWorker*>(req->data);
  worker->WorkComplete();
  delete worker;
}

void AsyncQueueWorker (AsyncWorker* worker) {
  uv_queue_work(
      uv_default_loop()
    , &worker->request
    , AsyncExecute
    , (uv_after_work_cb)AsyncExecuteComplete
  );
}

} // namespace nlmdb
