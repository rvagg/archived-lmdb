/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#ifndef LD_DATABASE_H
#define LD_DATABASE_H

#include <map>
#include <vector>
#include <node.h>
#include <nan.h>

#include "leveldown.h"
#include "iterator.h"

namespace leveldown {

#define DEFAULT_MAPSIZE 10 << 20 // 10 MB
#define DEFAULT_READERS 126 // LMDB default
#define DEFAULT_SYNC true
#define DEFAULT_READONLY false
#define DEFAULT_WRITEMAP false
#define DEFAULT_METASYNC true
#define DEFAULT_MAPASYNC false
#define DEFAULT_FIXEDMAP false
#define DEFAULT_NOTLS true

typedef struct OpenOptions {
  bool     createIfMissing;
  bool     errorIfExists;
  uint64_t mapSize;
  uint64_t maxReaders;
  bool     sync;
  bool     readOnly;
  bool     writeMap;
  bool     metaSync;
  bool     mapAsync;
  bool     fixedMap;
  bool     notls;
} OpenOptions;

NAN_METHOD(LevelDOWN);

struct Reference {
  Nan::Persistent<v8::Object> handle;
  MDB_val val;

  Reference(v8::Local<v8::Value> obj, MDB_val val) : val(val) {
    v8::Local<v8::Object> _obj = Nan::New<v8::Object>();
    _obj->Set(Nan::New("obj").ToLocalChecked(), obj);
    handle.Reset(_obj);
  };
};

static inline void ClearReferences (std::vector<Reference *> *references) {
  for (std::vector<Reference *>::iterator it = references->begin()
      ; it != references->end()
      ; ) {
    DisposeStringOrBufferFromSlice((*it)->handle, (*it)->val);
    it = references->erase(it);
  }
  delete references;
}

/* abstract */ class BatchOp {
 public:
  BatchOp (v8::Local<v8::Object> &keyHandle, MDB_val key);
  virtual ~BatchOp ();
  virtual int Execute (MDB_txn *txn, MDB_dbi dbi) =0;

 protected:
  Nan::Persistent<v8::Object> persistentHandle;
  MDB_val key;
};

class Database : public Nan::ObjectWrap {
public:
  static void Init ();
  static v8::Local<v8::Value> NewInstance (v8::Local<v8::String> &location);

  md_status OpenDatabase (OpenOptions options);
  void CloseDatabase     ();
  int PutToDatabase      (MDB_val key, MDB_val value);
  int PutToDatabase      (std::vector< BatchOp* >* operations);
  int GetFromDatabase    (MDB_val key, std::string& value);
  int DeleteFromDatabase (MDB_val key);
  int NewCursor          (MDB_txn **txn, MDB_cursor **cursor);
  void ReleaseIterator   (uint32_t id);
  uint64_t ApproximateSizeFromDatabase (MDB_val* start, MDB_val* end);
  void GetPropertyFromDatabase (char* property, std::string* value);
  int BackupDatabase (char* path);

  Database (const v8::Local<v8::Value>& from);
  ~Database ();

  MDB_dbi dbi;

private:
  MDB_env *env;
  Nan::Utf8String* location;
  uint32_t currentIteratorId;
  void(*pendingCloseWorker);

  std::map< uint32_t, leveldown::Iterator * > iterators;

  static NAN_METHOD(New);
  static NAN_METHOD(Open);
  static NAN_METHOD(Close);
  static NAN_METHOD(Put);
  static NAN_METHOD(Delete);
  static NAN_METHOD(Get);
  static NAN_METHOD(Batch);
  static NAN_METHOD(Write);
  static NAN_METHOD(Iterator);
  static NAN_METHOD(ApproximateSize);
  static NAN_METHOD(GetProperty);
  static NAN_METHOD(Backup);
};

} // namespace leveldown

#endif
