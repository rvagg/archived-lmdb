/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#ifndef NL_DATABASE_H
#define NL_DATABASE_H

#include "nlmdb.h"

namespace nlmdb {

NL_SYMBOL ( option_createIfMissing , createIfMissing ); // for open()
NL_SYMBOL ( option_errorIfExists   , errorIfExists   ); // for open()

NL_SYMBOL(option_asBuffer, asBuffer); // for get()

v8::Handle<v8::Value> NLMDB (const v8::Arguments& args);

class Database : public node::ObjectWrap {
public:
  static void Init ();
  static v8::Handle<v8::Value> NewInstance (const v8::Arguments& args);

  md_status OpenDatabase (bool createIfMissing, bool errorIfExists);
  int PutToDatabase (MDB_val key, MDB_val value);
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
  /*
  NL_V8_METHOD( Batch    )
  NL_V8_METHOD( Write    )
  */
};

} // namespace nlmdb

#endif
