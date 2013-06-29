/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_ITERATOR_H
#define NL_ITERATOR_H

#include <node.h>

#include "nlmdb.h"
#include "database.h"
#include "async.h"

namespace nlmdb {

NL_SYMBOL ( option_start         , start         );
NL_SYMBOL ( option_end           , end           );
NL_SYMBOL ( option_limit         , limit         );
NL_SYMBOL ( option_reverse       , reverse       );
NL_SYMBOL ( option_keys          , keys          );
NL_SYMBOL ( option_values        , values        );
NL_SYMBOL ( option_keyAsBuffer   , keyAsBuffer   );
NL_SYMBOL ( option_valueAsBuffer , valueAsBuffer );

v8::Handle<v8::Value> CreateIterator (const v8::Arguments& args);

class Iterator : public node::ObjectWrap {
public:
  static void Init ();
  static v8::Handle<v8::Object> NewInstance (
      v8::Handle<v8::Object> database
    , v8::Handle<v8::Number> id
    , v8::Handle<v8::Object> optionsObj
  );

  Iterator (
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
  );

  ~Iterator ();

  int          Next    (MDB_val *key, MDB_val *value);
  void         End     ();
  void         Release ();

private:
  Database    *database;
  uint32_t     id;
  MDB_txn     *txn;
  MDB_cursor  *cursor;
  std::string *start;
  std::string *end;
  bool         reverse;
  bool         keys;
  bool         values;
  int          limit;
  int          count;

public:
  bool         keyAsBuffer;
  bool         valueAsBuffer;
  bool         started;
  bool         nexting;
  bool         ended;
  AsyncWorker* endWorker;

private:
  int          GetIterator ();

  static v8::Persistent<v8::Function> constructor;

  NL_V8_METHOD( New  )
  NL_V8_METHOD( Next )
  NL_V8_METHOD( End  )
};

} // namespace nlmdb

#endif
