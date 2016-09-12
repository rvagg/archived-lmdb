/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#ifndef LD_DATABASE_ASYNC_H
#define LD_DATABASE_ASYNC_H

#include <vector>
#include <node.h>

#include "async.h"

namespace leveldown {

class OpenWorker : public AsyncWorker {
public:
  OpenWorker (
      Database *database
    , Nan::Callback *callback
    , OpenOptions options
  );

  virtual ~OpenWorker ();
  virtual void Execute ();

private:
  OpenOptions options;
};

class CloseWorker : public AsyncWorker {
public:
  CloseWorker (
      Database *database
    , Nan::Callback *callback
  );

  virtual ~CloseWorker ();
  virtual void Execute ();
  virtual void WorkComplete ();
};

class IOWorker    : public AsyncWorker {
public:
  IOWorker (
      Database *database
    , Nan::Callback *callback
    , MDB_val key
    , v8::Local<v8::Object> &keyHandle
  );

  virtual ~IOWorker ();
  virtual void WorkComplete ();

protected:
  MDB_val key;
  v8::Local<v8::Object> &keyHandle;
};

class ReadWorker : public IOWorker {
public:
  ReadWorker (
      Database *database
    , Nan::Callback *callback
    , MDB_val key
    , bool asBuffer
    , bool fillCache
    , v8::Local<v8::Object> &keyHandle
  );

  virtual ~ReadWorker ();
  virtual void Execute ();
  virtual void HandleOKCallback ();

private:
  bool asBuffer;
  std::string value;
};

class DeleteWorker : public IOWorker {
public:
  DeleteWorker (
      Database *database
    , Nan::Callback *callback
    , MDB_val key
    , bool sync
    , v8::Local<v8::Object> &keyHandle
  );

  virtual ~DeleteWorker ();
  virtual void Execute ();
  virtual void WorkComplete ();

protected:
};

class WriteWorker : public DeleteWorker {
public:
  WriteWorker (
      Database *database
    , Nan::Callback *callback
    , MDB_val key
    , MDB_val value
    , bool sync
    , v8::Local<v8::Object> &keyHandle
    , v8::Local<v8::Object> &valueHandle
  );

  virtual ~WriteWorker ();
  virtual void Execute ();
  virtual void WorkComplete ();

private:
  MDB_val value;
  v8::Local<v8::Object> &valueHandle;
};

class ApproximateSizeWorker : public AsyncWorker {
public:
  ApproximateSizeWorker (
      Database *database
    , Nan::Callback *callback
    , MDB_val* start
    , MDB_val* end
  );

  virtual ~ApproximateSizeWorker ();
  virtual void Execute ();
  virtual void HandleOKCallback ();

  private:
    MDB_val* start;
    MDB_val* end;
    uint64_t size;
};

class BackupWorker : public AsyncWorker {
public:
  BackupWorker (
      Database *database
    , Nan::Callback *callback
    , Nan::Utf8String* path
  );

  virtual ~BackupWorker ();
  virtual void Execute ();
  virtual void HandleOKCallback ();

  private:
    Nan::Utf8String* path;
};

} // namespace leveldown

#endif
