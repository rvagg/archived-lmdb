/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include "nlmdb.h"
#include "database.h"
#include "batch.h"

namespace nlmdb {

void Init (v8::Handle<v8::Object> target) {
  Database::Init();
  WriteBatch::Init();

  v8::Local<v8::Function> nlmdb =
      v8::FunctionTemplate::New(NLMDB)->GetFunction();

  target->Set(v8::String::NewSymbol("nlmdb"), nlmdb);
}

NODE_MODULE(nlmdb, Init)

} // namespace nlmdb
