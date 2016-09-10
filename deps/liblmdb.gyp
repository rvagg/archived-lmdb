{
    'targets': [{
        'target_name': 'liblmdb'
      , 'variables': {
            'lmdbversion': '20160205'
        }
      , 'defines': [ 'MDB_DEBUG=1' ]
      , 'type': 'static_library'
      , 'standalone_static_library': 1
      , 'direct_dependent_settings': {
            'include_dirs': [
                'liblmdb-<(lmdbversion)'
            ]
        }
      , 'conditions': [
            ['OS == "linux"', {
                'cflags': [
                    '-Waddress'
                  , '-Wno-unused-but-set-variable'
                ]
            }]
        ]
      , 'sources': [
            'liblmdb-<(lmdbversion)/mdb.c'
          , 'liblmdb-<(lmdbversion)/midl.c'
        ]
      , 'test-sources': [
            'liblmdb-<(lmdbversion)/mtest2.c'
          , 'liblmdb-<(lmdbversion)/mtest3.c'
          , 'liblmdb-<(lmdbversion)/mtest4.c'
          , 'liblmdb-<(lmdbversion)/mtest5.c'
          , 'liblmdb-<(lmdbversion)/mtest6.c'
          , 'liblmdb-<(lmdbversion)/mtest.c'
          , 'liblmdb-<(lmdbversion)/sample-mdb.c'
        ]
    }]
}
