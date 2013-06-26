/* Copyright (c) 2013 Rod Vagg
 * MIT +no-false-attribs License <https://github.com/rvagg/lmdb/blob/master/LICENSE>
 */

#include "batch.h"
#include "batch_async.h"

namespace nlmdb {

BatchWriteWorker::BatchWriteWorker (
    WriteBatch* batch
  , v8::Persistent<v8::Function> callback
) : AsyncWorker(batch->database, callback)
  , batch(batch)
{};

BatchWriteWorker::~BatchWriteWorker () {
  //delete batch;
}

void BatchWriteWorker::Execute () {
  status.code = database->PutToDatabase(batch->operations);
}

} // namespace nlmdb
