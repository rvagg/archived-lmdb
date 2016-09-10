/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#ifndef LD_ASYNC_H
#define LD_ASYNC_H

#include <node.h>
#include <nan.h>
#include "database.h"

namespace leveldown {

class Database;

/* abstract */ class AsyncWorker : public Nan::AsyncWorker {
public:
  AsyncWorker (
      leveldown::Database* database
    , Nan::Callback *callback
  ) : Nan::AsyncWorker(callback), database(database) { }

protected:
  void SetStatus (md_status status) {
    this->status = status;

    if (status.error.length() != 0) {
      char *e = new char[status.error.length() + 1];
      strcpy(e, status.error.c_str());
      SetErrorMessage(e);
    } else if (status.code != 0) {
      const char *me = mdb_strerror(status.code);
      char *e = new char[strlen(me) + 1];
      strcpy(e, me);
      SetErrorMessage(e);
    }
  }

  void SetStatus (int code) {
    md_status status;
    status.code = code;
    SetStatus(status);
  }

  void SetStatus (const char *err) {
    md_status status;
    status.code = 0;
    status.error = std::string(err);
    SetStatus(status);
  }

  Database* database;
  md_status status;
};

} // namespace leveldown

#endif
