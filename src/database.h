/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_DATABASE_H
#define NL_DATABASE_H

#include <vector>

#include "nlmdb.h"

namespace nlmdb {

NL_SYMBOL ( option_createIfMissing , createIfMissing ); // for open()
NL_SYMBOL ( option_errorIfExists   , errorIfExists   ); // for open()

NL_SYMBOL(option_asBuffer, asBuffer); // for get()

v8::Handle<v8::Value> NLMDB (const v8::Arguments& args);

struct Reference {
  v8::Persistent<v8::Value> ptr;
  MDB_val val;
  Reference(v8::Persistent<v8::Value> ptr, MDB_val val) :
      ptr(ptr)
    , val(val) { };
};

static inline void ClearReferences (std::vector<Reference>* references) {
  for (std::vector<Reference>::iterator it = references->begin()
      ; it != references->end()
      ; ) {
    DisposeStringOrBufferFromMDVal(it->ptr, it->val);
    it = references->erase(it);
  }
  delete references;
}

/* abstract */ class BatchOp {
 public:
  BatchOp (v8::Persistent<v8::Value> keyPtr, MDB_val key);
  virtual ~BatchOp ();
  virtual int Execute (MDB_txn *txn, MDB_dbi dbi) =0;

 protected:
  v8::Persistent<v8::Value> keyPtr;
  MDB_val key;
};

class Database : public node::ObjectWrap {
public:
  static void Init ();
  static v8::Handle<v8::Value> NewInstance (const v8::Arguments& args);

  md_status OpenDatabase (bool createIfMissing, bool errorIfExists);
  int PutToDatabase (MDB_val key, MDB_val value);
  int PutToDatabase (std::vector< BatchOp* >* operations);
  int GetFromDatabase (MDB_val key, MDB_val& value);
  int DeleteFromDatabase (MDB_val key);
  const char* Location() const;

  Database (const char* location);
  ~Database ();

private:
  MDB_env *env;
  MDB_dbi dbi;

  const char* location;

  static v8::Persistent<v8::Function> constructor;

  NL_V8_METHOD( New      )
  NL_V8_METHOD( Open     )
  NL_V8_METHOD( Close    )
  NL_V8_METHOD( Put      )
  NL_V8_METHOD( Get      )
  NL_V8_METHOD( Delete   )
  NL_V8_METHOD( Batch    )
};

} // namespace nlmdb

#endif
