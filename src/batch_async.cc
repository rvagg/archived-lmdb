/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */


#include "batch.h"
#include "batch_async.h"

namespace leveldown {

/** NEXT WORKER **/

BatchWriteWorker::BatchWriteWorker (
    WriteBatch* batch
  , Nan::Callback *callback
) : AsyncWorker(batch->database, callback)
  , batch(batch)
{};

BatchWriteWorker::~BatchWriteWorker () {}

void BatchWriteWorker::Execute () {
  SetStatus(database->PutToDatabase(batch->operations));
}

} // namespace leveldown
