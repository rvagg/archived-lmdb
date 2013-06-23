const tape = require('tape')
    , lmdb = require('./')

tape('lmdb', function (t) {
  var db = lmdb('./blerg')
  db.open(function (err) {
    t.notOk(err, 'no error')
    db.put('foo', 'bar', function (err) {
      t.notOk(err, 'no error')
      db.get('foo', { asBuffer: false }, function (err, value) {
        t.notOk(err, 'no error')
        t.equal(value, 'bar', 'correct value')
        db.del('foo', function (err) {
          t.notOk(err, 'no error')
          db.get('foo', { asBuffer: false }, function (err, value) {
            t.ok(err, 'no value')
            t.ok(/MDB_NOTFOUND:/.test(err.toString()), 'correct error')
            t.notOk(value, 'no value')
            db.close(function (err) {
              t.notOk(err, 'no error')
              t.end()
            })
          })
        })
      })
    })
  })
})