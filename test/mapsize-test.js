const test       = require('tape')
    , lmdb       = require('../')
    , testCommon = require('abstract-leveldown/testCommon')
      // 1MBish
    , bigBlob    = require('crypto').randomBytes(1000000)
    //, bigBlob    = Array.apply(null, Array(1024 * 100)).map(function () { return 'aaaaaaaaaa' }).join('')

test('setUp common', testCommon.setUp)

test('test default mapSize should bork', function (t) {
  var db   = lmdb(testCommon.location())
    , puts = 20
    , fails = 0
    , donePuts = 0
    , done = function () {
        if (++donePuts == puts) {
          t.ok(fails > 8 && fails <= 11, 'got expected number of fails (' + fails + ')')
          db.close(testCommon.tearDown.bind(null, t))
        }
      }
  db.open(function (err) {
    t.notOk(err, 'no error')

    for (var i = 0; i < puts; i++) {
      (function (i) {
        db.put(i, bigBlob, function (err) {
          err && fails++
          done()
        })
      }(i))
    }
  })
})

test('test mapSize increase', function (t) {
  var db   = lmdb(testCommon.location())
    , puts = 20
    , donePuts = 0
    , done = function () {
        if (++donePuts == puts)
          db.close(testCommon.tearDown.bind(null, t))
      }

  // 25MB
  db.open({ mapSize: 25 << 20 }, function (err) {
    t.notOk(err, 'no error')

    for (var i = 0; i < puts; i++) {
      (function (i) {
        db.put(i, bigBlob, function (err) {
          t.notOk(err, 'no error from large put #' + i)
          done()
        })
      }(i))
    }
  })
})
