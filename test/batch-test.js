const test       = require('tap').test
    , testCommon = require('abstract-leveldown/testCommon')
    , leveldown  = require('../')
    , abstract   = require('abstract-leveldown/abstract/batch-test')

abstract.all(leveldown, test, testCommon)