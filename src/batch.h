#ifndef LD_BATCH_H
#define LD_BATCH_H

#include <vector>
#include <node.h>

#include "database.h"

namespace leveldown {

class BatchDel : public BatchOp {
 public:
  BatchDel (v8::Local<v8::Object> &keyHandle, MDB_val key);
  virtual ~BatchDel ();
  virtual int Execute (MDB_txn *txn, MDB_dbi dbi);
};

class BatchPut : public BatchOp {
public:
  BatchPut (
      v8::Local<v8::Object> &keyHandle
    , MDB_val key
    , v8::Local<v8::Object> &valueHandle
    , MDB_val value
  );

  virtual ~BatchPut ();
  virtual int Execute (MDB_txn *txn, MDB_dbi dbi);

protected:
  MDB_val value;
};

class WriteBatch : public Nan::ObjectWrap {
public:
  static void Init();
  static v8::Local<v8::Value> NewInstance (
      v8::Local<v8::Object> database
    , v8::Local<v8::Object> optionsObj
  );

  WriteBatch  (Database* database, bool sync);
  ~WriteBatch ();

  void Put    (
      v8::Local<v8::Object> &keyHandle
    , MDB_val key
    , v8::Local<v8::Object> &valueHandle
    , MDB_val value
  );
  void Delete (v8::Local<v8::Object> &keyHandle, MDB_val key);
  void Clear  ();
  void Write  (v8::Local<v8::Function> callback);

  std::vector< BatchOp* >* operations;
  Database* database;
  bool sync;

private:
  bool written;

  static NAN_METHOD(New);
  static NAN_METHOD(Put);
  static NAN_METHOD(Del);
  static NAN_METHOD(Clear);
  static NAN_METHOD(Write);
};

} // namespace leveldown

#endif
