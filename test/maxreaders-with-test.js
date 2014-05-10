const test       = require('tap').test
    , lmdb       = require('../')
    , testCommon = require('abstract-leveldown/testCommon')

test('setUp common', testCommon.setUp)

test('test maxReaders increase', function (t) {
  var db   = lmdb(testCommon.location())
    // The default maxreaders is 126, so having that many active iterators
    // saturates all slots unless the max value is changed.
    , nConcurrentIterators = 126
    , nDone = 0;

    var setupDone = function(iteratorCreationError) {
        if (++nDone == nConcurrentIterators) {
          t.notOk(iteratorCreationError, 'no iterator creation error')

          // Create one last iterator. This errors if the readers table is
          // full, succeeds otherwise
          var iter = db.iterator();
          iter.next(function(err) {
            t.notOk(err, 'last concurrent reader was fine');

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

  db.open({ maxReaders : 200 }, function (err) {
    t.notOk(err, 'no db open error')

    // Create saturating iterators and `next` once to make sure they're active
    // and hence occupying a reader slot.
    for (var i = 0; i < nConcurrentIterators; i++) {
      db.iterator().next(setupDone);
    }

  })
})
