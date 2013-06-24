const test      = require('tap').test
    , leveldown = require('../')
    , abstract  = require('abstract-leveldown/abstract/leveldown-test')

abstract.args(leveldown, test)