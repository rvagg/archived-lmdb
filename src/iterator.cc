/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <nan.h>

#include "database.h"
#include "iterator.h"
#include "iterator_async.h"
#include "common.h"

namespace leveldown {

static Nan::Persistent<v8::FunctionTemplate> iterator_constructor;

Iterator::Iterator (
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
) : database(database)
  , id(id)
  , start(start)
  , end(end)
  , reverse(reverse)
  , keys(keys)
  , values(values)
  , limit(limit)
  , lt(lt)
  , lte(lte)
  , gt(gt)
  , gte(gte)
  , highWaterMark(highWaterMark)
  , keyAsBuffer(keyAsBuffer)
  , valueAsBuffer(valueAsBuffer)
{
  Nan::HandleScope scope;

  started    = false;
  rc         = database->NewCursor(&txn, &cursor);
  alloc      = rc == 0;
  count      = 0;
  seeking    = false;
  nexting    = false;
  ended      = false;
  endWorker  = NULL;
};

Iterator::~Iterator () {
  LD_FREE_COPY(start);
  LD_FREE_COPY(end);
  LD_FREE_COPY(lt);
  LD_FREE_COPY(gt);
  LD_FREE_COPY(lte);
  LD_FREE_COPY(gte);
};

bool Iterator::GetIterator () {
  if (!started) {
    started = true;

    if (!alloc)
      return false;

    if (start != NULL) {
      Seek(start);

      if (reverse) {
        if (rc == MDB_NOTFOUND) {
          // if it's past the last key, step back
          SeekToLast();
        } else if (rc == 0) {
          if (lt != NULL) {
            if (CompareRev(lt) <= 0)
              Prev();
          } else if (lte != NULL) {
            if (CompareRev(lte) < 0)
              Prev();
          } else if (start != NULL) {
            if (CompareRev(start))
              Prev();
          }
        }

        if (IsValid() && lt != NULL) {
          if (CompareRev(lt) <= 0)
            Prev();
        }
      } else {
        if (IsValid() && gt != NULL
            && CompareRev(gt) == 0)
          Next();
      }
    } else if (reverse) {
      SeekToLast();
    } else {
      SeekToFirst();
    }

    return true;
  }
  return false;
}

bool Iterator::Read (std::string& key, std::string& value) {
  // if it's not the first call, move to next item.
  if (!GetIterator() && !seeking) {
    if (!IsValid())
      return false;
    if (reverse)
      Prev();
    else
      Next();
  }

  seeking = false;

  // now check if this is the end or not, if not then return the key & value
  if (IsValid()) {
    int isEnd = end == NULL ? 1 : CompareRev(end);

    if ((limit < 0 || ++count <= limit)
      && (end == NULL
          || (reverse && (isEnd <= 0))
          || (!reverse && (isEnd >= 0)))
      && ( lt  != NULL ? (CompareRev(lt) > 0)
         : lte != NULL ? (CompareRev(lte) >= 0)
         : true )
      && ( gt  != NULL ? (CompareRev(gt) < 0)
         : gte != NULL ? (CompareRev(gte) <= 0)
         : true )
    ) {
      if (keys)
        key.assign((char *)currentKey.mv_data, currentKey.mv_size);
      if (values)
        value.assign((char *)currentValue.mv_data, currentValue.mv_size);
      return true;
    }
    // rc = MDB_NOTFOUND;
  }

  return false;
}

bool Iterator::IteratorNext (std::vector<std::pair<std::string, std::string> >& result) {
  size_t size = 0;
  while(true) {
    std::string key, value;
    bool ok = Read(key, value);

    if (ok) {
      result.push_back(std::make_pair(key, value));
      size = size + key.size() + value.size();

      if (size > highWaterMark)
        return true;

    } else {
      return false;
    }
  }
}

void Iterator::IteratorEnd () {
  if (alloc) {
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
  }
}

void Iterator::Release () {
  database->ReleaseIterator(id);
}

void checkEndCallback (Iterator* iterator) {
  iterator->nexting = false;
  if (iterator->endWorker != NULL) {
    Nan::AsyncQueueWorker(iterator->endWorker);
    iterator->endWorker = NULL;
  }
}

int Iterator::Compare (MDB_val* b) {
  return mdb_cmp(txn, database->dbi, &currentKey, b);
}

int Iterator::CompareRev (MDB_val* a) {
  return mdb_cmp(txn, database->dbi, a, &currentKey);
}

void Iterator::Seek (MDB_val* k) {
  currentKey.mv_data = k->mv_data;
  currentKey.mv_size = k->mv_size;
  rc = mdb_cursor_get(cursor, &currentKey, &currentValue, MDB_SET_RANGE);
}

void Iterator::Prev () {
  rc = mdb_cursor_get(cursor, &currentKey, &currentValue, MDB_PREV);
}

void Iterator::Next () {
  rc = mdb_cursor_get(cursor, &currentKey, &currentValue, MDB_NEXT);
}

void Iterator::SeekToFirst () {
  rc = mdb_cursor_get(cursor, &currentKey, &currentValue, MDB_FIRST);
}

void Iterator::SeekToLast () {
  rc = mdb_cursor_get(cursor, &currentKey, &currentValue, MDB_LAST);
}

bool Iterator::IsValid () {
  return rc == 0;
}

NAN_METHOD(Iterator::Seek) {
  Iterator* iterator = Nan::ObjectWrap::Unwrap<Iterator>(info.This());

  iterator->GetIterator();

  if (!iterator->IsValid()) {
    info.GetReturnValue().Set(info.Holder());
    return;
  }

  Nan::Utf8String key(info[0]);

  MDB_val k;
  k.mv_data = (void*)*key;
  k.mv_size = key.length();

  iterator->Seek(&k);
  iterator->seeking = true;

  if (iterator->IsValid()) {
    int cmp = iterator->Compare(&k);
    if (cmp > 0 && iterator->reverse) {
      iterator->Prev();
    } else if (cmp < 0 && !iterator->reverse) {
      iterator->Next();
    }
  } else {
    if (iterator->reverse) {
      iterator->SeekToLast();
    } else {
      iterator->SeekToFirst();
    }
    if (iterator->IsValid()) {
      int cmp = iterator->Compare(&k);
      if (cmp > 0 && iterator->reverse) {
        iterator->SeekToFirst();
        if (iterator->IsValid())
          iterator->Prev();
      } else if (cmp < 0 && !iterator->reverse) {
        iterator->SeekToLast();
        if (iterator->IsValid())
          iterator->Next();
      }
    }
  }

  info.GetReturnValue().Set(info.Holder());
}

NAN_METHOD(Iterator::Next) {
  Iterator* iterator = Nan::ObjectWrap::Unwrap<Iterator>(info.This());

  if (info.Length() == 0 || !info[0]->IsFunction()) {
    return Nan::ThrowError("next() requires a callback argument");
  }

  v8::Local<v8::Function> callback = info[0].As<v8::Function>();

  if (iterator->ended) {
    LD_RETURN_CALLBACK_OR_ERROR(callback, "cannot call next() after end()")
  }

  if (iterator->nexting) {
    LD_RETURN_CALLBACK_OR_ERROR(callback, "cannot call next() before previous next() has completed")
  }

  NextWorker* worker = new NextWorker(
      iterator
    , new Nan::Callback(callback)
    , checkEndCallback
  );
  // persist to prevent accidental GC
  v8::Local<v8::Object> _this = info.This();
  worker->SaveToPersistent("iterator", _this);
  iterator->nexting = true;
  Nan::AsyncQueueWorker(worker);

  info.GetReturnValue().Set(info.Holder());
}

NAN_METHOD(Iterator::End) {
  Iterator* iterator = Nan::ObjectWrap::Unwrap<Iterator>(info.This());

  if (info.Length() == 0 || !info[0]->IsFunction()) {
    return Nan::ThrowError("end() requires a callback argument");
  }

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(info[0]);

  if (iterator->ended) {
    LD_RETURN_CALLBACK_OR_ERROR(callback, "cannot call end() twice")
  } else {
    EndWorker* worker = new EndWorker(
        iterator
      , new Nan::Callback(callback)
    );
    // persist to prevent accidental GC
    v8::Local<v8::Object> _this = info.This();
    worker->SaveToPersistent("iterator", _this);
    iterator->ended = true;

    if (iterator->nexting) {
      // waiting for a next() to return, queue the end
      iterator->endWorker = worker;
    } else {
      Nan::AsyncQueueWorker(worker);
    }
  }

  info.GetReturnValue().Set(info.Holder());
}

void Iterator::Init () {
  v8::Local<v8::FunctionTemplate> tpl =
      Nan::New<v8::FunctionTemplate>(Iterator::New);
  iterator_constructor.Reset(tpl);
  tpl->SetClassName(Nan::New("Iterator").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetPrototypeMethod(tpl, "seek", Iterator::Seek);
  Nan::SetPrototypeMethod(tpl, "next", Iterator::Next);
  Nan::SetPrototypeMethod(tpl, "end", Iterator::End);
}

v8::Local<v8::Object> Iterator::NewInstance (
        v8::Local<v8::Object> database
      , v8::Local<v8::Number> id
      , v8::Local<v8::Object> optionsObj
    ) {

  Nan::EscapableHandleScope scope;

  Nan::MaybeLocal<v8::Object> maybeInstance;
  v8::Local<v8::Object> instance;
  v8::Local<v8::FunctionTemplate> constructorHandle =
      Nan::New<v8::FunctionTemplate>(iterator_constructor);

  if (optionsObj.IsEmpty()) {
    v8::Local<v8::Value> argv[2] = { database, id };
    maybeInstance = Nan::NewInstance(constructorHandle->GetFunction(), 2, argv);
  } else {
    v8::Local<v8::Value> argv[3] = { database, id, optionsObj };
    maybeInstance = Nan::NewInstance(constructorHandle->GetFunction(), 3, argv);
  }

  if (maybeInstance.IsEmpty())
    Nan::ThrowError("Could not create new Iterator instance");
  else
    instance = maybeInstance.ToLocalChecked();

  return scope.Escape(instance);
}

NAN_METHOD(Iterator::New) {
  Database* database = Nan::ObjectWrap::Unwrap<Database>(info[0]->ToObject());

  MDB_val* start = NULL;
  MDB_val* end = NULL;
  int limit = -1;
  // default highWaterMark from Readble-streams
  size_t highWaterMark = 16 * 1024;

  v8::Local<v8::Value> id = info[1];

  v8::Local<v8::Object> optionsObj;

  v8::Local<v8::Object> ltHandle;
  v8::Local<v8::Object> lteHandle;
  v8::Local<v8::Object> gtHandle;
  v8::Local<v8::Object> gteHandle;

  MDB_val* lt = NULL;
  MDB_val* lte = NULL;
  MDB_val* gt = NULL;
  MDB_val* gte = NULL;

  //default to forward.
  bool reverse = false;

  if (info.Length() > 1 && info[2]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(info[2]);

    reverse = BooleanOptionValue(optionsObj, "reverse");

    if (optionsObj->Has(Nan::New("start").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("start").ToLocalChecked()))
          || optionsObj->Get(Nan::New("start").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> startBuffer = optionsObj->Get(Nan::New("start").ToLocalChecked());

      // ignore start if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(startBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(start, startBuffer, start)
      }
    }

    if (optionsObj->Has(Nan::New("end").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("end").ToLocalChecked()))
          || optionsObj->Get(Nan::New("end").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> endBuffer = optionsObj->Get(Nan::New("end").ToLocalChecked());

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(endBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(end, endBuffer, end)
      }
    }

    if (!optionsObj.IsEmpty() && optionsObj->Has(Nan::New("limit").ToLocalChecked())) {
      limit = v8::Local<v8::Integer>::Cast(optionsObj->Get(
          Nan::New("limit").ToLocalChecked()))->Value();
    }

    if (optionsObj->Has(Nan::New("highWaterMark").ToLocalChecked())) {
      highWaterMark = v8::Local<v8::Integer>::Cast(optionsObj->Get(
            Nan::New("highWaterMark").ToLocalChecked()))->Value();
    }

    if (optionsObj->Has(Nan::New("lt").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("lt").ToLocalChecked()))
          || optionsObj->Get(Nan::New("lt").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> ltBuffer = optionsObj->Get(Nan::New("lt").ToLocalChecked());

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(ltBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(lt, ltBuffer, lt)
        if (reverse) {
          LD_FREE_COPY(start);
          LD_CREATE_COPY(start, lt);
        }
      }
    }

    if (optionsObj->Has(Nan::New("lte").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("lte").ToLocalChecked()))
          || optionsObj->Get(Nan::New("lte").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> lteBuffer = optionsObj->Get(Nan::New("lte").ToLocalChecked());

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(lteBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(lte, lteBuffer, lte)
        if (reverse) {
          LD_FREE_COPY(start);
          LD_CREATE_COPY(start, lte);
        }
      }
    }

    if (optionsObj->Has(Nan::New("gt").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("gt").ToLocalChecked()))
          || optionsObj->Get(Nan::New("gt").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> gtBuffer = optionsObj->Get(Nan::New("gt").ToLocalChecked());

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(gtBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(gt, gtBuffer, gt)
        if (!reverse) {
          LD_FREE_COPY(start);
          LD_CREATE_COPY(start, gt);
        }
      }
    }

    if (optionsObj->Has(Nan::New("gte").ToLocalChecked())
        && (node::Buffer::HasInstance(optionsObj->Get(Nan::New("gte").ToLocalChecked()))
          || optionsObj->Get(Nan::New("gte").ToLocalChecked())->IsString())) {

      v8::Local<v8::Value> gteBuffer = optionsObj->Get(Nan::New("gte").ToLocalChecked());

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(gteBuffer) > 0) {
        LD_STRING_OR_BUFFER_TO_COPY(gte, gteBuffer, gte)
        if (!reverse) {
          LD_FREE_COPY(start);
          LD_CREATE_COPY(start, gte);
        }
      }
    }

  }

  bool keys = BooleanOptionValue(optionsObj, "keys", true);
  bool values = BooleanOptionValue(optionsObj, "values", true);
  bool keyAsBuffer = BooleanOptionValue(optionsObj, "keyAsBuffer", true);
  bool valueAsBuffer = BooleanOptionValue(optionsObj, "valueAsBuffer", true);
  bool fillCache = BooleanOptionValue(optionsObj, "fillCache");

  Iterator* iterator = new Iterator(
      database
    , (uint32_t)id->Int32Value()
    , start
    , end
    , reverse
    , keys
    , values
    , limit
    , lt
    , lte
    , gt
    , gte
    , fillCache
    , keyAsBuffer
    , valueAsBuffer
    , highWaterMark
  );
  iterator->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

} // namespace leveldown
