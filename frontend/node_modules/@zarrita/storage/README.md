# @zarrita/storage

[![NPM](https://img.shields.io/npm/v/@zarrita/storage/next.svg?color=black)](https://www.npmjs.com/package/zarrita)
[![License](https://img.shields.io/npm/l/zarrita.svg?color=black)](https://github.com/manzt/zarrita.js/raw/main/LICENSE)

> Storage backends for Zarr in the browser, Node.js, Bun.js, and Deno.

## Installation

```sh
npm install @zarrita/storage
```

## Usage

```javascript
import * as zarr from "zarrita";
import { FetchStore, FileSystemStore } from "@zarrita/storage";

let remoteStore = new FetchStore("http://localhost:8080/data.zarr");
let arr = await zarr.open(remoteStore, { kind: "array" });

let localStore = new FileSystemStore("data.zarr");
await zarr.create(localStore, {
	data_type: "int64",
	shape: [100, 100],
	chunk_shape: [10, 10],
});
```

Read the [documentation](https://manzt.github.io/zarrita.js/) to learn more.

## License

MIT
