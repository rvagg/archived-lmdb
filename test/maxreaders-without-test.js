const test       = require('tap').test
    , lmdb       = require('../')
    , testCommon = require('abstract-leveldown/testCommon')

test('setUp common', testCommon.setUp)

test('test default maxReaders should bork', function (t) {
  var db   = lmdb(testCommon.location())
    // The default maxreaders is 126, so this should saturate all slots.
    , nConcurrentIterators = 126
    , nDone = 0;

    var setupDone = function(iteratorCreationError) {
        if (++nDone == nConcurrentIterators) {
          t.notOk(iteratorCreationError, 'no iterator creation error')

          // Create one last iterator. This should fail because the readers
          // table is full.
          var iter = db.iterator();
          iter.next(function(err) {
            var correctError = err
                === 'MDB_READERS_FULL: Environment maxreaders limit reached';
            t.ok(err, 'got read error from last concurrent reader');

            // Force exit; no cleanup
            // ----------------------
            //
            // Doing this instead of a proper
            //
            //     db.close(testCommon.tearDown.bind(null, t))
            //
            // because it's impossible to close an iterator from within the
            // callback to its `next`-call, which is where all this gets
            // called from.
            //
            // This doesn't affect the test's validity, as the reader limit
            // is what counts here.
            process.exit(0);
          })
        }
      }

  db.open(function (err) {
    t.notOk(err, 'no db open error')

    // Create saturating iterators and `next` once to make sure they're active
    // and hence occupying a reader slot.
    for (var i = 0; i < nConcurrentIterators; i++) {
      db.iterator().next(setupDone);
    }

  })
})
