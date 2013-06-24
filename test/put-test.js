const test       = require('tap').test
    , testCommon = require('abstract-leveldown/testCommon')
    , leveldown  = require('../')
    , abstract   = require('abstract-leveldown/abstract/put-test')

abstract.all(leveldown, test, testCommon)