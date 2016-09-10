/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#ifndef LD_ITERATOR_H
#define LD_ITERATOR_H

#include <node.h>
#include <vector>
#include <nan.h>

#include "leveldown.h"
#include "database.h"
#include "async.h"

namespace leveldown {

class Database;
class AsyncWorker;

class Iterator : public Nan::ObjectWrap {
public:
  static void Init ();
  static v8::Local<v8::Object> NewInstance (
      v8::Local<v8::Object> database
    , v8::Local<v8::Number> id
    , v8::Local<v8::Object> optionsObj
  );

  Iterator (
      Database* database
    , uint32_t id
    , MDB_val* start
    , MDB_val* end
    , bool reverse
    , bool keys
    , bool values
    , int limit
    , MDB_val* lt
    , MDB_val* lte
    , MDB_val* gt
    , MDB_val* gte
    , bool fillCache
    , bool keyAsBuffer
    , bool valueAsBuffer
    , size_t highWaterMark
  );

  ~Iterator ();

  bool IteratorNext (std::vector<std::pair<std::string, std::string> >& result);
  void IteratorEnd ();
  void Release ();

  int Compare (MDB_val* b);
  int CompareRev (MDB_val* a);
  void Seek (MDB_val* k);
  void Prev ();
  void Next ();
  void SeekToFirst ();
  void SeekToLast ();
  bool IsValid ();

private:
  Database* database;
  uint32_t id;
  MDB_txn     *txn;
  MDB_cursor  *cursor;
  MDB_val* start;
  MDB_val* end;
  MDB_val currentKey;
  MDB_val currentValue;
  bool seeking;
  bool reverse;
  bool keys;
  bool values;
  int limit;
  MDB_val* lt;
  MDB_val* lte;
  MDB_val* gt;
  MDB_val* gte;
  int count;
  size_t highWaterMark;

public:
  bool keyAsBuffer;
  bool valueAsBuffer;
  int rc;
  bool started;
  bool alloc;
  bool nexting;
  bool ended;
  AsyncWorker* endWorker;

private:
  bool Read (std::string& key, std::string& value);
  bool GetIterator ();

  static NAN_METHOD(New);
  static NAN_METHOD(Seek);
  static NAN_METHOD(Next);
  static NAN_METHOD(End);
};

} // namespace leveldown

#endif
