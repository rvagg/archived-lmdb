/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>
#include <node_buffer.h>
#include <iostream>

#include "database.h"
#include "nlmdb.h"
#include "async.h"
#include "iterator_async.h"

namespace nlmdb {

/** NEXT WORKER **/

NextWorker::NextWorker (
    Iterator* iterator
  , v8::Persistent<v8::Function> callback
  , void (*localCallback)(Iterator*)
) : AsyncWorker(NULL, callback)
  , iterator(iterator)
  , localCallback(localCallback)
{};

NextWorker::~NextWorker () {}

void NextWorker::Execute () {
//std::cerr << "NextWorker::Execute: " << iterator->id << std::endl;
  status.code = iterator->Next(&key, &value);
}

void NextWorker::WorkComplete () {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE
  /*
std::cerr << "NextWorker::WorkComplete: " << iterator->id <<
  ", status.code=" << status.code << std::endl;
  */
  if (status.code == MDB_NOTFOUND || (status.code == 0 && status.error.length() == 0))
    HandleOKCallback();
  else
    HandleErrorCallback();
  callback.Dispose(NL_NODE_ISOLATE);
}

void NextWorker::HandleOKCallback () {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

//std::cerr << "NextWorker::HandleOKCallback: " << iterator->id << std::endl;
//std::cerr << "Read [" << (char*)key.mv_data << "]=[" << (char*)value.mv_data << "]\n";

  if (status.code == MDB_NOTFOUND) {
    //std::cerr << "run callback, ended MDB_NOTFOUND\n";
    localCallback(iterator);
    NL_RUN_CALLBACK(callback, NULL, 0);
    scope.Close(v8::Undefined());
    return;
  }

  v8::Local<v8::Value> returnKey;
  if (iterator->keyAsBuffer) {
    returnKey = v8::Local<v8::Value>::New(
      node::Buffer::New((char*)key.mv_data, key.mv_size)->handle_
    );
  } else {
    returnKey = v8::String::New((char*)key.mv_data, key.mv_size);
  }

  v8::Local<v8::Value> returnValue;
  if (iterator->valueAsBuffer) {
    returnValue = v8::Local<v8::Value>::New(
      node::Buffer::New((char*)value.mv_data, value.mv_size)->handle_
    );
  } else {
    returnValue = v8::String::New((char*)value.mv_data, value.mv_size);
  }

  // clean up & handle the next/end state see iterator.cc/checkEndCallback
  //std::cerr << "run callback, ended FOUND\n";
  localCallback(iterator);

  v8::Local<v8::Value> argv[] = {
      v8::Local<v8::Value>::New(v8::Null())
    , returnKey
    , returnValue
  };
  NL_RUN_CALLBACK(callback, argv, 3);

  scope.Close(v8::Undefined());
}

/** END WORKER **/

EndWorker::EndWorker (
    Iterator* iterator
  , v8::Persistent<v8::Function> callback
) : AsyncWorker(NULL, callback)
  , iterator(iterator)
{executed=false;};

EndWorker::~EndWorker () {}

void EndWorker::Execute () {
  executed = true;
  //std::cerr << "EndWorker::Execute...\n";
  iterator->End();
}

void EndWorker::HandleOKCallback () {
  //std::cerr << "EndWorker::HandleOKCallback: " << iterator->id << std::endl;
  iterator->Release();
  NL_RUN_CALLBACK(callback, NULL, 0);
}

} // namespace nlmdb
