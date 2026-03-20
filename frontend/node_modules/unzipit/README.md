# unzipit.js

<img src="./unzipit-no-anim.png" style="max-width: 640px">

Random access unzip library for browser and node based JavaScript

[![Build Status](https://travis-ci.org/greggman/unzipit.svg?branch=master)](https://travis-ci.org/greggman/unzipit)
[[Live Tests](https://greggman.github.io/unzipit/test/)]

* Less than 8k gzipped without workers, Less than 13k with.
* [6x to 25x faster than JSZip](https://jsperf.com/jszip-vs-unzipit/4) without workers and even faster with
* Uses far less memory.
* Can [avoid downloading the entire zip file](#Streaming) if the server supports http range requests.

# How to use

Live Example: [https://jsfiddle.net/greggman/awez4sd7/](https://jsfiddle.net/greggman/awez4sd7/)
Live Parallel Example: [https://jsfiddle.net/greggman/cgdjm07f/](https://jsfiddle.net/greggman/cgdjm07f/)

## without workers

```js
import {unzip} from 'unzipit';

async function readFiles(url) {
  const {entries} = await unzip(url);

  // print all entries and their sizes
  for (const [name, entry] of Object.entries(entries)) {
    console.log(name, entry.size);
  }

  // read an entry as an ArrayBuffer
  const arrayBuffer = await entries['path/to/file'].arrayBuffer();

  // read an entry as a blob and tag it with mime type 'image/png'
  const blob = await entries['path/to/otherFile'].blob('image/png');
}
```

## with workers

```js
import {unzip, setOptions} from 'unzipit';

setOptions({workerURL: 'path/to/unzipit-worker.module.js'});

async function readFiles(url) {
  const {entries} = await unzip(url);
  ...
}
```

or if you prefer

```js
import * as unzipit from 'unzipit';

unzipit.setOptions({workerURL: 'path/to/unzipit-worker.module.js'});

async function readFiles(url) {
  const {entries} = await unzipit.unzip(url);
  ...
}
```

## In Parallel

```js
import {unzip, setOptions} from 'unzipit';

setOptions({workerURL: 'path/to/unzipit-worker.module.js'});

async function readFiles(url) {
  const {entries} = await unzipit.unzip(url);
  const names = Object.keys(entries);
  const blobs = await Promise.all(Object.values(entries).map(e => e.blob()));

  // names and blobs are now parallel arrays so do whatever you want.
  const blobsByName = Object.fromEntries(names.map((name, i) => [name, blobs[i]]));
}
```

You can also pass a [`Blob`](https://developer.mozilla.org/en-US/docs/Web/API/Blob),
[`ArrayBuffer`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer),
[`SharedArrayBuffer`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer),
[`TypedArray`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray),
or your own `Reader`

For using without a builder/bundler grab `unzipit.min.js` or `unzipit.module.js` from
the [`dist`](https://github.com/greggman/unzipit/tree/master/dist) folder and
include with

```js
import * as unzipit from `./unzipit.module.js`;
```

or

```html
<script src="unzipit.min.js"></script>
```

or vs CDN

```js
import * as unzipit from 'https://unpkg.com/unzipit@1.4.0/dist/unzipit.module.js';
```

or

```html
<script src="https://unpkg.com/unzipit@1.4.0/dist/unzipit.js"></script>
```
## Node

For node you need to make your own `Reader` or pass in an
[`ArrayBuffer`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer),
[`SharedArrayBuffer`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer),
or [`TypedArray`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray).

### Load a file as an ArrayBuffer

```js
const unzipit = require('unzipit');
const fsPromises = require('fs').promises;

async function readFiles(filename) {
  const buf = await fsPromises.readFile(filename);
  const {zip, entries} = await unzipit.unzip(new Uint8Array(buf));
  ... (see code above)
}
```

You can also pass your own reader. Here's 2 examples. This first one
is stateless. That means there is never anything to clean up. But,
it has the overhead of opening the source file once for each time
you get the contents of an entry. I have no idea what the overhead
of that is.

```js
const unzipit = require('unzipit');
const fsPromises = require('fs').promises;

class StatelessFileReader {
  constructor(filename) {
    this.filename = filename;
  }
  async getLength() {
    if (this.length === undefined) {
      const stat = await fsPromises.stat(this.filename);
      this.length = stat.size;
    }
    return this.length;
  }
  async read(offset, length) {
    const fh = await fsPromises.open(this.filename);
    const data = new Uint8Array(length);
    await fh.read(data, 0, length, offset);
    await fh.close();
    return data;
  }
}

async function readFiles(filename) {
  const reader = new StatelessFileReader(filename);
  const {zip, entries} = await unzipit.unzip(reader);
  ... (see code above)
}
```

Here's also an example of one that only opens the file a single time
but that means the file stays open until you manually call close.

```js
class FileReader {
  constructor(filename) {
    this.fhp = fsPromises.open(filename);
  }
  async close() {
    const fh = await this.fhp;
    await fh.close();
  }
  async getLength() {
    if (this.length === undefined) {
      const fh = await this.fhp;
      const stat = await fh.stat();
      this.length = stat.size;
    }
    return this.length;
  }
  async read(offset, length) {
    const fh = await this.fhp;
    const data = new Uint8Array(length);
    await fh.read(data, 0, length, offset);
    return data;
  }
}

async function doStuff() {
  // ...

  const reader = new FileReader(filename);
  const {zip, entries} = await unzipit.unzip(reader);

  // ... do stuff with entries ...

  // you must call reader.close for the file to close
  await reader.close();
}
```

### Workers in Node

```js
const unzipit = require('unzipit');

unzipit.setOptions({workerURL: require.resolve('unzipit/dist/unzipit-worker.js')});

...

// Only if you need node to exit you need to shut down the workers.
unzipit.cleanup();
```

## Why?

Most of the js libraries I looked at would decompress all files in the zip file.
That's probably the most common use case but it didn't fit my needs. I needed
to, as fast as possible, open a zip and read a specific file. The better libraries
only worked on node, I needed a browser based solution for Electron.

Note that to repo the behavior of most unzip libs would just be

```js
import {unzip} from 'unzipit';

async function readFiles(url) {
  const {entries} = await unzip(url);
  await Promise.all(Object.values(entries).map(async(entry) => {
    entry.data = await entry.arrayBuffer();
  }));
}
```

One other thing is that many libraries seem bloated. IMO the smaller the API the better.
I don't need a library to try to do 50 things via options and configuration. Rather I need
a library to handle the main task and make it possible to do the rest outside the library.
This makes a library far more flexible.

As an example some libraries provide no raw data for filenames. Apparently some zip files
have non-utf8 filenames in them. The solution for this library is to do that on your own.

Example

```js
const {zip, entriesArray} = await unzipit.unzipRaw(url);
// decode names as big5 (chinese)
const decoder = new TextDecoder('big5');
entriesArray.forEach(entry => {
  entry.name = decoder.decode(entry.nameBytes);
});
const entries = Object.fromEntries(entriesArray.map(v => [v.name, v]));
... // same as above beyond this point
```

Same thing with filenames. If you care about slashes or backslashes do that yourself outside the library

```js
const {entries} = await unzipit(url);
// change slashes and backslashes into '-'
entries.forEach(entry => {
  entry.name = entry.name.replace(/\\|\//g, '-');
});
```

Some libraries both zip and unzip.
IMO those should be separate libraries as there is little if any code to share between
both. Plenty of projects only need to do one or the other.

Similarly inflate and deflate libraries should be separate from zip, unzip libraries.
You need one or the other not both. See zlib as an example.

This library is ES6 based using async/await and import which makes the code
much simpler.

Advantages over other libraries.

* JSZIP requires the entire compressed file in memory.
  It also requires reading through all entries in order.

* UZIP requires the entire compressed file to be in memory and
  the entire uncompressed contents of all the files to be in memory.

* Yauzl does not require all the files to be in memory but
  they do have to be read in order and it has very peculiar API where
  you still have to manually go through all the entries even if
  you don't choose to read their contents. Further it's node only.

* fflate has 2 modes. One the entire contents of all
  uncompressed files are provided therefore using lots
  of memory. The other is like Yauzl where you're required
  to handle every file but you can choose to ignore
  certain ones. Further in this mode (maybe both modes) are 
  not standards compliant. It scans for files but that is not
  a valid way to read a zip file. The only valid way to read a zip file
  is to jump to the end of the file and find the table of
  contents. So, fflate will fail on perfectly valid zip files.

Unzipit does not require all compressed content nor all uncompressed
content to be in memory. Only the entries you access use memory. 
If you use a Blob as input the browser can effectively virtualize
access so it doesn't have to be in memory and unzipit will only
access the parts of the blob needed to read the content you request.

Further, if you use the `HTTPRangeReader` or similar, unzipit only
downloads/reads the parts of the zip file you actually use, saving you
bandwidth.

As well, if you only need the data for images or video or audio then you can do
things like

```js
const {entries} = await unzip(url);
const blob = await entries['/some/image.jpg'].blob('image/jpeg');
const url = URL.createObjectURL(blob);
const img = new Image();
img.src = url;
```

Notice there is no access to the data using Blobs which means the browser
manages them. They don't count as part of the JavaScript heap.

In node, the examples with the file readers will only read the header and whatever entries' contents
you ask for so similarly you can avoid having everything in memory except the things you read.


# API

```js
import { unzipit, unzipitRaw, setOptions, cleanup } from 'unzipit';
```

# unzip, unzipRaw

```js
async unzip(url: string): ZipInfo
async unzip(src: Blob): ZipInfo
async unzip(src: TypedArray): ZipInfo
async unzip(src: ArrayBuffer): ZipInfo
async unzip(src: Reader): ZipInfo

async unzipRaw(url: string): ZipInfoRaw
async unzipRaw(src: Blob): ZipInfoRaw
async unzipRaw(src: TypedArray): ZipInfoRaw
async unzipRaw(src: ArrayBuffer): ZipInfoRaw
async unzipRaw(src: Reader): ZipInfoRaw
```

`unzip` and `unzipRaw` are async functions that take a url, `Blob`, `TypedArray`, or `ArrayBuffer` or a `Reader`.
Both functions return an object with fields `zip` and `entries`.
The difference is with `unzip` the `entries` is an object mapping filenames to `ZipEntry`s where as `unzipRaw` it's
an array of `ZipEntry`s. The reason to use `unzipRaw` over `unzip` is if the filenames are not utf8
then the library can't make an object from the names. In that case you get an array of entries, use `entry.nameBytes`
and decode the names as you please.

```js
type ZipInfo = {
  zip: Zip,
  entries: {[key: string]: ZipEntry},
};
```

```js
type ZipInfoRaw = {
  zip: Zip,
  entries: [ZipEntry],
};
```

```js
class Zip {
  comment: string,           // the comment for the zip file
  commentBytes: Uint8Array,  // the raw data for comment, see nameBytes
}
```

```js
class ZipEntry {
  async blob(type?: string): Blob,  // returns a Blob for this entry
                                    //  (optional type as in 'image/jpeg')
  async arrayBuffer(): ArrayBuffer, // returns an ArrayBuffer for this entry
  async text(): string,             // returns text, assumes the text is valid utf8.
                                    // If you want more options decode arrayBuffer yourself
  async json(): any,                // returns text with JSON.parse called on it.
                                    // If you want more options decode arrayBuffer yourself
  name: string,                     // name of entry
  nameBytes: Uint8Array,            // raw name of entry (see notes)
  size: number,                     // size in bytes
  compressedSize: number,           // size before decompressing
  comment: string,                  // the comment for this entry
  commentBytes: Uint8Array,         // the raw comment for this entry
  lastModDate: Date,                // a Date
  isDirectory: bool,                // True if directory
  encrypted: bool,                  // True if encrypted
  externalFileAttributes: number,   // platform specific file attributes
  versionMadeBy: number,            // platform that made this file
}
```

```js
interface Reader {
  async getLength(): number,
  async read(offset, size): Uint8Array,
}
```

## setOptions

```js
setOptions(options: UnzipitOptions)
```

The options are

* `useWorkers`: true/false (default: false)

* `workerURL`: string

  The URL to use to load the worker script. Note setting this automatically sets `useWorkers` to true

* `numWorkers`: number (default: 1)

  How many workers to use. You can inflate more files in parallel with more workers.

## cleanup

```js
cleanup()
```

Shuts down the workers. You would only need to call this if you want node
to exit since it will wait for the workers to exit.

# Notes:

## Supporting old browsers

Use a transpiler like [Babel](https://babeljs.io).

## Caching

If you ask for the same entry twice it will be read twice and decompressed twice.
If you want to cache entires implement that at a level above unzipit

## Streaming

You can't stream zip files. The only valid way to read a zip file is to read the
central directory which is at the end of the zip file. Sure there are zip files
where you can cheat and read the local headers of each file but that is an invalid
way to read a zip file and it's trivial to create zip files that will fail when
read that way but are perfectly valid zip files.

If your server supports http range requests you can do this.

```js
import {unzip, HTTPRangeReader} from 'unzipit';

async function readFiles(url) {
  const reader = new HTTPRangeReader(url);
  const {zip, entries} = await unzip(reader);
  // ... access the entries as normal
}
```

## Special headers and options for network requests

The library takes a URL but there are no options for cors, or credentials etc.
If you need that pass in a Blob or ArrayBuffer you fetched yourself.

```js
import {unzip} from 'unzipit';

...

const req = await fetch(url, { mode: 'cors' });
const blob = await req.blob();
const {entries} = await unzip(blob);
```

## Non UTF-8 Filenames

The zip standard predates unicode so it's possible and apparently not uncommon for files
to have non-unicode names. `entry.nameBytes` contains the raw bytes of the filename.
so you are free to decode the name using your own methods. See example above.

## Filename issues in general.

unzipit doesn't and can't know if a filename is valid for your use case. A zip file
can have any name with any characters in the filename data. All unzipit can do is give you
the filename as a string from the zip file. It's up to you do deal with it, for example
to strip out or replace characters in the filename that are incompatible with your OS.
For example [this zip file](https://github.com/greggman/unzipit/files/10998616/problem-filenames.zip)
has these filenames:  `'this#file\\name%is&iffy'`, `'???And This one???'`, `'fo:oo'` which
I believe are problematic on Windows. A user found a file with double slashes as in `foo//bar`
so you'll need to decide what to do with that.

There is also the issue a user could make a malicious filename. For example "../../.bash_profile"
on the hope that some program doesn't check the names and just uses the paths as is.
If you're going to use unzipit to create files you should check and sanitize your paths.

## ArrayBuffer and SharedArrayBuffer caveats

If you pass in an `ArrayBuffer` or `SharedArrayBuffer` you need to keep the data unchanged
until you're finished using the data. The library doesn't make a copy, it uses the buffer directly.

## Handling giant entries

There is no way for the library to know what "too large" means to you.
The simple way to handle entries that are too large is to check their
size before asking for their content.

```js
  const kMaxSize = 1024*1024*1024*2;  // 2gig
  if (entry.size > kMaxSize) {
    throw new Error('this entry is larger than your max supported size');
  }
  const data = await entry.arrayBuffer();
  ...
```

## Encrypted, Password protected Files

unzipit does not currently support encrypted zip files and will throw if you try to get the data for one.
Put it on the TODO list 😅

## File Attributes

If you want to make an unzip utilitiy using this library you'll need to be able to mark some files as executable.
That is unforutunately platform specific. For example, Windows has no concept of "mark a file as executable".
Each zip entry provides a `versionMadeBy` and `externalFileAttributes` property. You could theoretically use
that to set file attributes. For example

```js
fs.writeFileSync(filename, data);
if (process.platform === 'darwin' || process.platform === 'linux') {
  const platform = entry.versionMadeBy >> 8;
  const unix = 3;
  const darwin = 13
  if (entry.versionMadeBy === unix || entry.versionMadeBy === darwin) {
    // no idea what's best here
    //                                                 +- owner read
    //                                                 |+- owner write
    //                                                 ||+- owner execute
    //                                                 |||+- group read
    //                                                 ||||+- group write
    //                                                 |||||+- group execute
    //                                                 ||||||+- other read
    //                                                 |||||||+- other write
    //                                                 ||||||||+- other execute
    //                                                 |||||||||
    //                                                 VVVVVVVVV
    let mod = (entry.externalFileAttributes >> 16) & 0b111111111;  // all the bits
    mod &= 0b111100100;   // remove write and executable from group and other?
    mod |= 0b110100100;   // add in owner R/W, group R, other R
    fs.chmodSync(filename, mod);
  }
}
```

## Other Limitations

unzipit only supports the uncompressed and deflate compression algorithms. Other algorithms are defined
in the zip spec but are uncommon.

# Testing

When writing tests serve the folder with your favorite web server (recommend [`servez`](https://www.npmjs.com/package/servez))
then go to `http://localhost:8080/test/` to easily re-run the tests. You can set a grep regular expression to only run certain tests
`http://localhost:8080/test/?grep=json`. It's up to you to encode the regular expression for a URL. For example

```js
encodeURIComponent('j(.*?)son')
"j(.*%3F)son"
```

so `http://localhost:8080/test/?grep=j(.*%3F)son`. The regular expression will be marked as case insensitive.

Of course you can also `npm test` to run the tests from the command line.

## Debugging

Follow the instructions on testing but add  `?timeout=0` to the URL as in `http://localhost:8080/tests/?timeout=0`

## Live Browser Tests

[https://greggman.github.io/unzipit/test/](https://greggman.github.io/unzipit/test/)

# Acknowledgements

* The code is **heavily** based on [yauzl](https://github.com/thejoshwolfe/yauzl)
* The code uses the es6 module version of [uzip.js](https://www.npmjs.com/package/uzip-module)

# Licence

MIT
