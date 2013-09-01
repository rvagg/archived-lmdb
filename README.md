LMDB for Node.js
================

A Low-level, [LevelDOWN](https://github.com/rvagg/node-leveldown)-compatible, Node.js LMDB binding
--------------------------------------------------------------------------------------------------

[![NPM](https://nodei.co/npm/lmdb.png?stars&downloads)](https://nodei.co/npm/lmdb/) [![NPM](https://nodei.co/npm-dl/lmdb.png)](https://nodei.co/npm/lmdb/)

LMDB for Node.js, is primarily designed to serve as a back-end to **[LevelUP](https://github.com/rvagg/node-levelup)**, it is strongly recommended that you use LevelUP in preference to LMDB directly.

<a name="platforms"></a>
Tested & supported platforms
----------------------------

  * **Linux**
  * *Others... testing son*

<a name="api"></a>
## API

  * <a href="#ctor"><code><b>lmdb()</b></code></a>
  * <a href="#lmdb_open"><code><b>lmdb#open()</b></code></a>
  * <a href="#lmdb_close"><code><b>lmdb#close()</b></code></a>
  * <a href="#lmdb_put"><code><b>lmdb#put()</b></code></a>
  * <a href="#lmdb_get"><code><b>lmdb#get()</b></code></a>
  * <a href="#lmdb_del"><code><b>lmdb#del()</b></code></a>
  * <a href="#lmdb_batch"><code><b>lmdb#batch()</b></code></a>
  * <a href="#lmdb_approximateSize"><code><b>lmdb#approximateSize()</b></code></a>
  * <a href="#lmdb_getProperty"><code><b>lmdb#getProperty()</b></code></a>
  * <a href="#lmdb_iterator"><code><b>lmdb#iterator()</b></code></a>
  * <a href="#iterator_next"><code><b>iterator#next()</b></code></a>
  * <a href="#iterator_end"><code><b>iterator#end()</b></code></a>
  * <a href="#lmdb_destroy"><code><b>lmdb.destroy()</b></code></a>
  * <a href="#lmdb_repair"><code><b>lmdb.repair()</b></code></a>


--------------------------------------------------------
<a name="ctor"></a>
### lmdb(location)
<code>lmdb()</code> returns a new Node.js **LMDB** instance. `location` is a String pointing to the LMDB store to be opened or created.


--------------------------------------------------------
<a name="lmdb_open"></a>
### lmdb#open([options, ]callback)
<code>open()</code> is an instance method on an existing database object.

The `callback` function will be called with no arguments when the database has been successfully opened, or with a single `error` argument if the open operation failed for any reason.

#### `options`

The optional `options` argument may contain:

* `'createIfMissing'` *(boolean, default: `true`)*: If `true`, will initialise an empty data store at the specified location if one doesn't already exist. If `false` and a database doesn't exist you will receive an error in your `open()` callback and your database won't open.

* `'errorIfExists'` *(boolean, default: `false`)*: If `true`, you will receive an error in your `open()` callback if the database exists at the specified location.

* `'mapSize'` *(boolean, default: `10485760` (10MB))*: Specify the size of the memory map, which is also the **maximum size of the database**. The value should be chosen as large as possible, to accommodate future growth of the database. The size may be changed by closing and reopening the environment. Any attempt to set a size smaller than the space already consumed by the environment will be silently changed to the current size of the used space. The size should be a multiple of the OS page size.

* `'sync'` *(boolean, default: `true`)*: By default, system buffers are flushed to disk after committing transactions (which are performed on every operation). Use this option to turn off this behaviour to speed up writes at the risk of losing writes upon system crash. Note that setting `'sync'` to `false` and `'writeMap'` to `true` leaves the system with no hint for when to write transactions to disk. Using `'mapAsync'` with `'writeMap'` may be preferable.

* `'readOnly'` *(boolean, default: `false`)*: Open the environment in read-only mode. No write operations will be allowed. LMDB will still modify the lock file - except on read-only filesystems, where LMDB does not use locks.

* `'writeMap'` *(boolean, default: `false`)*: Use a writeable *memory map* (unless `'readOnly'` is set). This is faster and uses fewer `malloc` operations. Note that setting `'writeMap'` to `true` will involve the pre-allocation of the data store, as a single file, of `'mapSize'` size.

* `'metaSync'` *(boolean, default: `true`)*: System buffers will be flushed to disk once per transaction (unless `'sync'` is set to `false`). Setting `'metaSync'` to `false` will prevent a metadata flush, deferring it until the system flushes files to disk, or the next (non-read-only) write. A `'metaSync'` set to `false` will improve performance and maintain database integrity, but a system crash may undo the last committed transaction.

* `'mapAsync'` *(boolean, default: `false`)*: When using a `'writeMap'`, use asynchronous flushes to disk. As with `'sync'` set to `false`, a system crash can then corrupt the database or lose the last transactions.


--------------------------------------------------------
<a name="lmdb_close"></a>
### lmdb#close(callback)
<code>close()</code> is an instance method on an existing database object. The underlying LMDB database will be closed and the `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.


--------------------------------------------------------
<a name="lmdb_put"></a>
### lmdb#put(key, value[, options], callback)
<code>put()</code> is an instance method on an existing database object, used to store new entries, or overwrite existing entries in the LMDB store.

The `key` and `value` objects may either be `String`s or Node.js `Buffer` objects and cannot be `undefined` or `null`. Other object types are converted to JavaScript `String`s with the `toString()` method and the resulting `String` *may not* be a zero-length. A richer set of data-types are catered for in LevelUP.

The `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.


--------------------------------------------------------
<a name="lmdb_get"></a>
### lmdb#get(key[, options], callback)
<code>get()</code> is an instance method on an existing database object, used to fetch individual entries from the LMDB store.

The `key` object may either be a `String` or a Node.js `Buffer` object and cannot be `undefined` or `null`. Other object types are converted to JavaScript `String`s with the `toString()` method and the resulting `String` *may not* be a zero-length. A richer set of data-types are catered for in LevelUP.

#### `options`

* `'asBuffer'` *(boolean, default: `true`)*: Used to determine whether to return the `value` of the entry as a `String` or a Node.js `Buffer` object. Note that converting from a `Buffer` to a `String` incurs a cost so if you need a `String` (and the `value` can legitimately become a UFT8 string) then you should fetch it as one with `asBuffer: true` and you'll avoid this conversion cost.

The `callback` function will be called with a single `error` if the operation failed for any reason. If successful the first argument will be `null` and the second argument will be the `value` as a `String` or `Buffer` depending on the `asBuffer` option.


--------------------------------------------------------
<a name="lmdb_del"></a>
### lmdb#del(key[, options], callback)
<code>del()</code> is an instance method on an existing database object, used to delete entries from the LMDB store.

The `key` object may either be a `String` or a Node.js `Buffer` object and cannot be `undefined` or `null`. Other object types are converted to JavaScript `String`s with the `toString()` method and the resulting `String` *may not* be a zero-length. A richer set of data-types are catered for in LevelUP.

The `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.


--------------------------------------------------------
<a name="lmdb_batch"></a>
### lmdb#batch(operations[, options], callback)
<code>batch()</code> is an instance method on an existing database object. Used for very fast bulk-write operations (both *put* and *delete*). The `operations` argument should be an `Array` containing a list of operations to be executed sequentially, although as a whole they are executed within a single transaction on LMDB. Each operation is contained in an object having the following properties: `type`, `key`, `value`, where the *type* is either `'put'` or `'del'`. In the case of `'del'` the `'value'` property is ignored. Any entries with a `'key'` of `null` or `undefined` will cause an error to be returned on the `callback` and any `'type': 'put'` entry with a `'value'` of `null` or `undefined` will return an error. See [LevelUP](https://github.com/rvagg/node-levelup#batch) for full documentation on how this works in practice.

The `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.


--------------------------------------------------------
<a name="lmdb_iterator"></a>
### lmdb#iterator([options])
<code>iterator()</code> is an instance method on an existing database object. It returns a new **Iterator** instance which abstracts an LMDB **"cursor"**.

#### `options`

The optional `options` object may contain:

* `'start'`: the key you wish to start the read at. By default it will start at the beginning of the store. Note that the *start* doesn't have to be an actual key that exists, LMDB will simply jump to the *next* key, greater than the key you provide.

* `'end'`: the key you wish to end the read on. By default it will continue until the end of the store. Again, the *end* doesn't have to be an actual key as an (inclusive) `<=`-type operation is performed to detect the end. You can also use the `destroy()` method instead of supplying an `'end'` parameter to achieve the same effect.

* `'reverse'` *(boolean, default: `false`)*: a boolean, set to true if you want the stream to go in reverse order.

* `'keys'` *(boolean, default: `true`)*: whether the callback to the `next()` method should receive a non-null `key`. There is a small efficiency gain if you ultimately don't care what the keys are as they don't need to be converted and copied into JavaScript.

* `'values'` *(boolean, default: `true`)*: whether the callback to the `next()` method should receive a non-null `value`. There is a small efficiency gain if you ultimately don't care what the values are as they don't need to be converted and copied into JavaScript.

* `'limit'` *(number, default: `-1`)*: limit the number of results collected by this iterator. This number represents a *maximum* number of results and may not be reached if you get to the end of the store or your `'end'` value first. A value of `-1` means there is no limit.

* `'keyAsBuffer'` *(boolean, default: `true`)*: Used to determine whether to return the `key` of each entry as a `String` or a Node.js `Buffer` object. Note that converting from a `Buffer` to a `String` incurs a cost so if you need a `String` (and the `value` can legitimately become a UFT8 string) then you should fetch it as one.

* `'valueAsBuffer'` *(boolean, default: `true`)*: Used to determine whether to return the `value` of each entry as a `String` or a Node.js `Buffer` object.


--------------------------------------------------------
<a name="iterator_next"></a>
### iterator#next(callback)
<code>next()</code> is an instance method on an existing iterator object, used to increment the underlying LMDB cursor and return the entry at that location.

the `callback` function will be called with no arguments in any of the following situations:

* the iterator comes to the end of the store
* the `end` key has been reached; or
* the `limit` has been reached

Otherwise, the `callback` function will be called with the following 3 arguments:

* `error` - any error that occurs while incrementing the iterator.
* `key` - either a `String` or a Node.js `Buffer` object depending on the `keyAsBuffer` argument when the `createIterator()` was called.
* `value` - either a `String` or a Node.js `Buffer` object depending on the `valueAsBuffer` argument when the `createIterator()` was called.


--------------------------------------------------------
<a name="iterator_end"></a>
### iterator#end(callback)
<code>end()</code> is an instance method on an existing iterator object. The underlying LMDB cursor will be deleted and the `callback` function will be called with no arguments if the operation is successful or with a single `error` argument if the operation failed for any reason.


<a name="support"></a>
Getting support
---------------

There are multiple ways you can find help in using LMDB / LevelUP / LevelDB in Node.js:

 * **IRC:** you'll find an active group of LevelUP users in the **##leveldb** channel on Freenode, including most of the contributors to this project.
 * **Mailing list:** there is an active [Node.js LevelUP](https://groups.google.com/forum/#!forum/node-levelup) Google Group.
 * **GitHub:** you're welcome to open an issue here on this GitHub repository if you have a question.

<a name="contributing"></a>
Contributing
------------

Node.js LMDB is an **OPEN Open Source Project**. This means that:

> Individuals making significant and valuable contributions are given commit-access to the project to contribute as they see fit. This project is more like an open wiki than a standard guarded open source project.

See the [CONTRIBUTING.md](https://github.com/rvagg/lmdb/blob/master/CONTRIBUTING.md) file for more details.


<a name="licence"></a>
Licence &amp; copyright
-------------------

Copyright (c) 2012-2013 Node.js LMDB contributors.

Node.js LMDB is licensed under an MIT +no-false-attribs license. All rights not explicitly granted in the MIT license are reserved. See the included LICENSE file for more details.

*Node.js LMDB builds on the excellent work of the Howard Chu of Symas Corp and additional contributors. LMDB are both issued under the [The OpenLDAP Public License](http://www.OpenLDAP.org/license.html).*