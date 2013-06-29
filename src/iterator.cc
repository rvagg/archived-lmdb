/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include <node.h>
#include <node_buffer.h>
#include <iostream>
#include <string.h>

#include "database.h"
#include "iterator.h"
#include "iterator_async.h"

namespace nlmdb {

inline int compare(std::string *str, MDB_val *value) {
  const int min_len = (str->length() < value->mv_size)
      ? str->length()
      : value->mv_size;
  int r = memcmp(str->c_str(), (char *)value->mv_data, min_len);
  if (r == 0) {
    if (str->length() < value->mv_size) r = -1;
    else if (str->length() > value->mv_size) r = +1;
  }
  return r;
}

Iterator::Iterator (
    Database    *database
  , uint32_t     id
  , std::string *start
  , std::string *end
  , bool         reverse
  , bool         keys
  , bool         values
  , int          limit
  , bool         keyAsBuffer
  , bool         valueAsBuffer
) : database(database)
  , id(id)
  , start(start)
  , end(end)
  , reverse(reverse)
  , keys(keys)
  , values(values)
  , limit(limit)
  , keyAsBuffer(keyAsBuffer)
  , valueAsBuffer(valueAsBuffer)
{
  count     = 0;
  started   = false;
  nexting   = false;
  ended     = false;
  endWorker = NULL;
};

Iterator::~Iterator () {
  if (start != NULL)
    delete start;
  if (end != NULL)
    delete end;
};

int Iterator::Next (MDB_val *key, MDB_val *value) {
  //std::cerr << "Iterator::Next " << started << ", " << id << std::endl;
  int rc = 0;

  if (!started) {
    rc = database->NewIterator(&txn, &cursor);
    //std::cerr << "opened cursor!! " << cursor << ", " << strerror(rc) << std::endl;
    if (rc) {
      //std::cerr << "returning 0: " << rc << std::endl;
      return rc;
    }

    if (start != NULL) {
      key->mv_data = (void*)start->data();
      key->mv_size = start->length();
      rc = mdb_cursor_get(cursor, key, value, MDB_SET_RANGE);
      if (reverse) {
        if (rc == MDB_NOTFOUND)
          rc = mdb_cursor_get(cursor, key, value, MDB_LAST);
        else if (rc == 0 && compare(start, key))
          rc = mdb_cursor_get(cursor, key, value, MDB_PREV);
      }
    } else if (reverse) {
      rc = mdb_cursor_get(cursor, key, value, MDB_LAST);
    } else {
      rc = mdb_cursor_get(cursor, key, value, MDB_FIRST);
    }

    started = true;
    //std::cerr << "Started " << started << std::endl;
  } else {
    if (reverse)
      rc = mdb_cursor_get(cursor, key, value, MDB_PREV);
    else
      rc = mdb_cursor_get(cursor, key, value, MDB_NEXT);
  }

  if (rc) {
    //std::cerr << "returning 1: " << rc << std::endl;
    return rc;
  }

  /*
  std::cerr << "***" << std::string((const char*)key->mv_data, key->mv_size) << std::endl;
  if (end != NULL)
    std::cerr << "***end=" << end->c_str() << ", " << reverse << ", " << compare(end, key) << std::endl;
    */

  // 'end' here is an inclusive test
  if ((limit < 0 || ++count <= limit)
      && (end == NULL
          || (reverse && compare(end, key) <= 0)
          || (!reverse && compare(end, key) >= 0))) {
    return 0; // good to continue
  }

  key = 0;
  value = 0;
  return MDB_NOTFOUND;
}

void Iterator::End () {
  //std::cerr << "Iterator::End " << started << ", " << id << std::endl;
  if (started) {
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
  }
}

void Iterator::Release () {
  //std::cerr << "Iterator::Release " << started << ", " << id << std::endl;
  database->ReleaseIterator(id);
}

void checkEndCallback (Iterator* iterator) {
  iterator->nexting = false;
  if (iterator->endWorker != NULL) {
    AsyncQueueWorker(iterator->endWorker);
    iterator->endWorker = NULL;
  }
}

v8::Handle<v8::Value> Iterator::Next (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  Iterator* iterator = node::ObjectWrap::Unwrap<Iterator>(args.This());

  if (args.Length() == 0 || !args[0]->IsFunction()) {
    NL_THROW_RETURN(next() requires a callback argument)
  }

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[0]);

  if (iterator->ended) {
    NL_RETURN_CALLBACK_OR_ERROR(callback, "cannot call next() after end()")
  }

  if (iterator->nexting) {
    NL_RETURN_CALLBACK_OR_ERROR(callback, "cannot call next() before previous next() has completed")
  }

  NextWorker* worker = new NextWorker(
      iterator
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
    , checkEndCallback
  );
  iterator->nexting = true;
  AsyncQueueWorker(worker);

  return scope.Close(args.Holder());
}

v8::Handle<v8::Value> Iterator::End (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  Iterator* iterator = node::ObjectWrap::Unwrap<Iterator>(args.This());
  //std::cerr << "Iterator::End" << iterator->id << ", " << iterator->nexting << ", " << iterator->ended << std::endl;

  if (args.Length() == 0 || !args[0]->IsFunction()) {
    NL_THROW_RETURN(end() requires a callback argument)
  }

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[0]);

  if (iterator->ended) {
    NL_RETURN_CALLBACK_OR_ERROR(callback, "end() already called on iterator")
  }

  EndWorker* worker = new EndWorker(
      iterator
    , v8::Persistent<v8::Function>::New(NL_NODE_ISOLATE_PRE callback)
  );
  iterator->ended = true;

  if (iterator->nexting) {
    // waiting for a next() to return, queue the end
    //std::cerr << "Iterator is nexting: " << iterator->id << std::endl;
    iterator->endWorker = worker;
  } else {
    //std::cerr << "Iterator can be ended: " << iterator->id << std::endl;
    AsyncQueueWorker(worker);
  }

  return scope.Close(args.Holder());
}

v8::Persistent<v8::Function> Iterator::constructor;

void Iterator::Init () {
  NL_NODE_ISOLATE_DECL
  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(New);
  tpl->SetClassName(v8::String::NewSymbol("Iterator"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("next")
    , v8::FunctionTemplate::New(Next)->GetFunction()
  );
  tpl->PrototypeTemplate()->Set(
      v8::String::NewSymbol("end")
    , v8::FunctionTemplate::New(End)->GetFunction()
  );
  constructor = v8::Persistent<v8::Function>::New(
      NL_NODE_ISOLATE_PRE
      tpl->GetFunction());
}

v8::Handle<v8::Object> Iterator::NewInstance (
        v8::Handle<v8::Object> database
      , v8::Handle<v8::Number> id
      , v8::Handle<v8::Object> optionsObj
    ) {

  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  v8::Local<v8::Object> instance;

  if (optionsObj.IsEmpty()) {
    v8::Handle<v8::Value> argv[2] = { database, id };
    instance = constructor->NewInstance(2, argv);
  } else {
    v8::Handle<v8::Value> argv[3] = { database, id, optionsObj };
    instance = constructor->NewInstance(3, argv);
  }

  return scope.Close(instance);
}

v8::Handle<v8::Value> Iterator::New (const v8::Arguments& args) {
  NL_NODE_ISOLATE_DECL
  NL_HANDLESCOPE

  Database* database = node::ObjectWrap::Unwrap<Database>(args[0]->ToObject());

  //TODO: remove this, it's only here to make NL_STRING_OR_BUFFER_TO_MDVAL happy
  v8::Handle<v8::Function> callback;

  std::string* start = NULL;
  std::string* end = NULL;
  int limit = -1;

  v8::Local<v8::Value> id = args[1];

  v8::Local<v8::Object> optionsObj;

  if (args.Length() > 1 && args[2]->IsObject()) {
    optionsObj = v8::Local<v8::Object>::Cast(args[2]);

    if (optionsObj->Has(option_start)
        && (node::Buffer::HasInstance(optionsObj->Get(option_start))
          || optionsObj->Get(option_start)->IsString())) {

      v8::Local<v8::Value> startBuffer =
          v8::Local<v8::Value>::New(optionsObj->Get(option_start));

      // ignore start if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(startBuffer) > 0) {
        NL_STRING_OR_BUFFER_TO_MDVAL(_start, startBuffer, start)
        start = new std::string((const char*)_start.mv_data, _start.mv_size);
      }
    }

    if (optionsObj->Has(option_end)
        && (node::Buffer::HasInstance(optionsObj->Get(option_end))
          || optionsObj->Get(option_end)->IsString())) {

      v8::Local<v8::Value> endBuffer =
          v8::Local<v8::Value>::New(optionsObj->Get(option_end));

      // ignore end if it has size 0 since a Slice can't have length 0
      if (StringOrBufferLength(endBuffer) > 0) {
        NL_STRING_OR_BUFFER_TO_MDVAL(_end, endBuffer, end)
        end = new std::string((const char*)_end.mv_data, _end.mv_size);
      }
    }

    if (!optionsObj.IsEmpty() && optionsObj->Has(option_limit)) {
      limit =
        v8::Local<v8::Integer>::Cast(optionsObj->Get(option_limit))->Value();
    }
  }

  bool reverse      = BooleanOptionValue(optionsObj, option_reverse);
  bool keys         = BooleanOptionValueDefTrue(optionsObj, option_keys);
  bool values       = BooleanOptionValueDefTrue(optionsObj, option_values);
  bool keyAsBuffer  = BooleanOptionValueDefTrue(optionsObj, option_keyAsBuffer);
  bool valueAsBuffer = BooleanOptionValueDefTrue(
      optionsObj
    , option_valueAsBuffer
  );

  Iterator* iterator = new Iterator(
      database
    , (uint32_t)id->Int32Value()
    , start
    , end
    , reverse
    , keys
    , values
    , limit
    , keyAsBuffer
    , valueAsBuffer
  );
  iterator->Wrap(args.This());

  //std::cerr << "New Iterator " << iterator->id << std::endl;

  return scope.Close(args.This());
}

} // namespace nlmdb
