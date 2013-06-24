/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_DATABASE_ASYNC_H
#define NL_DATABASE_ASYNC_H

#include <vector>
#include <node.h>

#include "async.h"

namespace nlmdb {

class OpenWorker : public AsyncWorker {
public:
  OpenWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , bool createIfMissing
    , bool errorIfExists
  );

  virtual ~OpenWorker ();
  virtual void Execute ();

private:
  bool createIfMissing;
  bool errorIfExists;
};

class CloseWorker : public AsyncWorker {
public:
  CloseWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
  );

  virtual ~CloseWorker ();
  virtual void Execute ();
  virtual void WorkComplete ();
};

class IOWorker    : public AsyncWorker {
public:
  IOWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , MDB_val key
    , v8::Persistent<v8::Value> keyPtr
  );

  virtual ~IOWorker ();
  virtual void WorkComplete ();

protected:
  MDB_val key;
  v8::Persistent<v8::Value> keyPtr;
};

class ReadWorker : public IOWorker {
public:
  ReadWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , MDB_val key
    , bool asBuffer
    , v8::Persistent<v8::Value> keyPtr
  );

  virtual ~ReadWorker ();
  virtual void Execute ();
  virtual void HandleOKCallback ();

private:
  bool asBuffer;
  MDB_val value;
};

class DeleteWorker : public IOWorker {
public:
  DeleteWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , MDB_val key
    , v8::Persistent<v8::Value> keyPtr
  );

  virtual ~DeleteWorker ();
  virtual void Execute ();

protected:
};

class WriteWorker : public DeleteWorker {
public:
  WriteWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , MDB_val key
    , MDB_val value
    , v8::Persistent<v8::Value> keyPtr
    , v8::Persistent<v8::Value> valuePtr
  );

  virtual ~WriteWorker ();
  virtual void Execute ();
  virtual void WorkComplete ();

private:
  MDB_val value;
  v8::Persistent<v8::Value> valuePtr;
};

/*
class BatchWorker : public AsyncWorker {
public:
  BatchWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , leveldb::WriteBatch* batch
    , std::vector<Reference>* references
    , bool sync
  );

  virtual ~BatchWorker ();
  virtual void Execute ();

private:
  leveldb::WriteOptions* options;
  leveldb::WriteBatch* batch;
  std::vector<Reference>* references;
};

class ApproximateSizeWorker : public AsyncWorker {
public:
  ApproximateSizeWorker (
      Database* database
    , v8::Persistent<v8::Function> callback
    , MDB_val start
    , MDB_val end
    , v8::Persistent<v8::Value> startPtr
    , v8::Persistent<v8::Value> endPtr
  );

  virtual ~ApproximateSizeWorker ();
  virtual void Execute ();
  virtual void HandleOKCallback ();
  virtual void WorkComplete ();

  private:
    leveldb::Range range;
    v8::Persistent<v8::Value> startPtr;
    v8::Persistent<v8::Value> endPtr;
    uint64_t size;
};
*/
} // namespace nlmdb

#endif
