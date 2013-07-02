/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_DATABASE_H
#define NL_DATABASE_H

#include <map>
#include <vector>
#include <node.h>

#include "nlmdb.h"

namespace nlmdb {

NL_SYMBOL ( option_createIfMissing , createIfMissing ); // for open()
NL_SYMBOL ( option_errorIfExists   , errorIfExists   ); // for open()
NL_SYMBOL ( option_mapSize         , mapSize         ); // for open()
#define DEFAULT_MAPSIZE 10 << 20 // 10 MB
NL_SYMBOL ( option_sync            , sync            ); // for open()
#define DEFAULT_SYNC true
NL_SYMBOL ( option_readOnly        , readOnly        ); // for open()
#define DEFAULT_READONLY false
NL_SYMBOL ( option_writeMap        , writeMap        ); // for open()
#define DEFAULT_WRITEMAP false
NL_SYMBOL ( option_metaSync        , metaSync        ); // for open()
#define DEFAULT_METASYNC true
NL_SYMBOL ( option_mapAsync        , mapAsync        ); // for open()
#define DEFAULT_MAPASYNC false
NL_SYMBOL ( option_fixedMap        , fixedMap        ); // for open()
#define DEFAULT_FIXEDMAP false
NL_SYMBOL ( option_asBuffer        , asBuffer        ); // for get()

typedef struct OpenOptions {
  bool     createIfMissing;
  bool     errorIfExists;
  uint32_t mapSize;
  bool     sync;
  bool     readOnly;
  bool     writeMap;
  bool     metaSync;
  bool     mapAsync;
  bool     fixedMap;
} OpenOptions;

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

  md_status OpenDatabase (OpenOptions options);
  int PutToDatabase      (MDB_val key, MDB_val value);
  int PutToDatabase      (std::vector< BatchOp* >* operations);
  int GetFromDatabase    (MDB_val key, MDB_val& value);
  int DeleteFromDatabase (MDB_val key);
  int NewIterator        (MDB_txn **txn, MDB_cursor **cursor);
  void ReleaseIterator   (uint32_t id);
  const char* Location() const;

  Database (const char* location);
  ~Database ();

private:
  MDB_env *env;
  MDB_dbi dbi;

  const char* location;
  uint32_t currentIteratorId;
  void(*pendingCloseWorker);

  std::map< uint32_t, v8::Persistent<v8::Object> > iterators;

  static v8::Persistent<v8::Function> constructor;

  NL_V8_METHOD( New      )
  NL_V8_METHOD( Open     )
  NL_V8_METHOD( Close    )
  NL_V8_METHOD( Put      )
  NL_V8_METHOD( Get      )
  NL_V8_METHOD( Delete   )
  NL_V8_METHOD( Batch    )
  NL_V8_METHOD( Iterator )
};

} // namespace nlmdb

#endif
