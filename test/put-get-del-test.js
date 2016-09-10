const test       = require('tape')
    , testCommon = require('abstract-leveldown/testCommon')
    , leveldown  = require('../')
    , fs         = require('fs')
    , path       = require('path')
    , testBuffer = new Buffer(100)
    , abstract   = require('abstract-leveldown/abstract/put-get-del-test')

testBuffer.fill(2);
abstract.all(leveldown, test, testCommon, testBuffer)
