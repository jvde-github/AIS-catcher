
# uzip-module

zlib inflate, deflate, inflateRaw, deflateRaw, as well
as simple in memory zip creation and parsing.

This is a ES6 module version of [UZIP.js](https://github.com/photopea/UZIP.js)

It's faster than [pako](https://github.com/nodeca/pako) in my tests.

# Usage

```
import {
  deflate,
  deflateRaw,
  inflate,
  inflateRaw,
  encode,
  parse,
} from 'uzip-module';
```

## deflate/inflate

### compress Uint8Array using deflate algorithm. 

Includes header and footer

```
const compressed = deflate(uint8Array);
```

or

```
const compressed = deflate(uint8Array, {level:9});
```

### decompress Uint8Array that has header and footer with delate algorithm

```
const uncompressedUint8Array = inflate(compressedUint8Array);
```

or

```
const uncompressedUint8Array = inflate(
    compressedUint8Array,
    destinationUint8Array);
```

## deflateRaw, inflateRaw

These take the exact same arguments as `inflate` and `deflate`
but don't store or expect the header or the footer

## encode/parse

### encode

Creates a zip file. You pass a JavaScript object of filenames
to Uint8Arrays it returns a Uint8Array zip file

```
const utf8Encoder = new TextEncoder();
const files = {
  'stuff/': utf8Encoder.encode(''),
  'stuff/dog.txt': utf8Encoder.encode('german shepard\n'),
  'stuff/birds/': utf8Encoder.encode(''),
  'stuff/birds/bird.txt': utf8Encoder.encode('parrot\n'),
  'stuff/cat.txt': utf8Encoder.encode('siamese\n'),
  'stuff/long.txt': utf8Encoder.encode(`${new Array(200).fill('compress').join('')}\n`),
}
const zipUint8Array = encode(files);
```

### parse

Does the opposite of encode. Takes a zip Uint8Array
and returns a JavaScript object of filenames to Uint8Arrays

Calling `parse` on the `zipUint8Array` from the previous
example will return the same data seen in `files` above

```
const unzippedFiles = parse(files);
```

You can also call it with an extra `true` in which case it will
just return the filenames and a `size` and `csize` for each one.
The uncompressed and compressed sizes;

```
const unzippedFileSizes = parse(files, true);
```

`unzippedFileSizes` would have a structure like

```
{
  "stuff/": {
    "size": 0,
    "csize": 0
  },
  "stuff/dog.txt": {
    "size": 15,
    "csize": 15
  },
  "stuff/long.txt": {
    "size": 1601,
    "csize": 24
  }
}
```

# Notes

All credit goes to the original author [Photopea](https://github.com/photopea/UZIP.js).

I ported this to ES6 modules to use in [another library](https://github.com/greggman/unzipit).
Originally I thought about putting the various parts in more separate files
like the inflate in one file, deflate in another, parse, and encode in others
but tree shaking is supposed to handle this stuff more or less
so I think just moving it to ES6 modules is enough to get any
unused code stripped. The only other think maybe left to do
is change the `bin` and `f` imports in `UZIP.js` to import each
individual identifier instead of all of them.

Also I spent about an hour trying to get ES6 modules to work with mocha in node
but failed so got sick of wasting time and used puppeteer. Patches welcome
to remove that dependency.

I didn't make a non-ES6 version. If it's important submit a PR (babel) and I'll
take a look. 
