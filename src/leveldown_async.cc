/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */

#include "leveldown.h"
#include "leveldown_async.h"

namespace leveldown {

/** DESTROY WORKER **/

DestroyWorker::DestroyWorker (
    Nan::Utf8String* location
  , Nan::Callback *callback
) : AsyncWorker(NULL, callback)
  , location(location)
{};

DestroyWorker::~DestroyWorker () {
  delete location;
}

void DestroyWorker::Execute () {
  MDB_env* env;
  MDB_txn *txn;
  MDB_dbi dbi;
  int rc;

  rc = mdb_env_create(&env);
  if (rc) {
    SetStatus(rc);
    return;
  }

  rc = mdb_env_open(env, **location, 0, 0664);
  if (rc) {
    SetStatus(rc);
    return;
  }

  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc) {
    SetStatus(rc);
    return;
  }

  rc = mdb_open(txn, NULL, 0, &dbi);
  if (rc) {
    mdb_txn_abort(txn);
    SetStatus(rc);
    return;
  }

  rc = mdb_drop(txn, dbi, 1);
  if (rc) {
    mdb_txn_abort(txn);
    SetStatus(rc);
    return;
  }

  rc = mdb_txn_commit(txn);
  if (rc) {
    SetStatus(rc);
    return;
  }

  mdb_env_close(env);
}

void DestroyWorker::WorkComplete () {
  Nan::HandleScope scope;

  if (status.code == 0 && status.error.length() == 0)
    HandleOKCallback();
  else
    HandleErrorCallback();
}

/** REPAIR WORKER **/

RepairWorker::RepairWorker (
    Nan::Utf8String* location
  , Nan::Callback *callback
) : AsyncWorker(NULL, callback)
  , location(location)
{};

RepairWorker::~RepairWorker () {
  delete location;
}

void RepairWorker::Execute () {
  SetStatus("Not implemented.");
}

void RepairWorker::WorkComplete () {
  Nan::HandleScope scope;

  if (status.code == 0 && status.error.length() == 0)
    HandleOKCallback();
  else
    HandleErrorCallback();
}

} // namespace leveldown
