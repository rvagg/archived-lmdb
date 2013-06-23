const tape = require('tape')
    , lmdb = require('./')

tape('lmdb', function (t) {
  t.plan(5)

  var db = lmdb('./blerg')
  db.open(function (err) {
    t.notOk(err, 'no error')
    console.error('opened!')
    db.put('foo', 'bar', function (err) {
    console.error('put!')
      t.notOk(err, 'no error')
      db.get('foo', { asBuffer: false }, function (err, value) {
        t.notOk(err, 'no error')
        t.equal(value, 'bar', 'correct value')
        db.close(function (err) {
          t.notOk(err, 'no error')
          t.end()
        })
      })
    })
  })
})