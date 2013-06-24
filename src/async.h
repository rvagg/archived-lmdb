/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_ASYNC_H
#define NL_ASYNC_H

#include <node.h>
#include "database.h"

namespace nlmdb {

/* abstract */ class AsyncWorker {
public:
  AsyncWorker (
      nlmdb::Database* database
    , v8::Persistent<v8::Function> callback
  );

  virtual ~AsyncWorker ();
  uv_work_t request;
  virtual void WorkComplete ();
  virtual void Execute () =0;

protected:
  Database* database;
  v8::Persistent<v8::Function> callback;
  md_status status;
  virtual void HandleOKCallback ();
  virtual void HandleErrorCallback ();
};

void AsyncExecute (uv_work_t* req);
void AsyncExecuteComplete (uv_work_t* req);
void AsyncQueueWorker (AsyncWorker* worker);

} // namespace nlmdb

#endif
