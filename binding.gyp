{
    "targets": [{
        "target_name": "nlmdb"
      , "dependencies": [
            "<(module_root_dir)/deps/liblmdb.gyp:liblmdb"
        ]
      , "sources": [
            "src/nlmdb.cc"
          , "src/async.cc"
          , "src/database.cc"
          , "src/database_async.cc"
        ]
    }]
}