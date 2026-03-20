export type ZipInfo = {
  zip: Zip,
  entries: {[key: string]: ZipEntry},
};

export type ZipInfoRaw = {
  zip: Zip,
  entries: ZipEntry[],
};

export type Zip = {
  comment: string,           // the comment for the zip file
  commentBytes: Uint8Array,  // the raw data for comment, see nameBytes
};

export type ZipEntry = {
  blob(type?: string): Promise<Blob>,  // returns a Blob for this entry
                                       //  (optional type as in 'image/jpeg')
  arrayBuffer(): Promise<ArrayBuffer>, // returns an ArrayBuffer for this entry
  text(): Promise<string>,             // returns text, assumes the text is valid utf8.
                                    // If you want more options decode arrayBuffer yourself
  json(): Promise<any>,                // returns text with JSON.parse called on it.
                                    // If you want more options decode arrayBuffer yourself
  name: string,                     // name of entry
  nameBytes: Uint8Array,            // raw name of entry (see notes)
  size: number,                     // size in bytes
  compressedSize: number,           // size before decompressing
  comment: string,                  // the comment for this entry
  commentBytes: Uint8Array,         // the raw comment for this entry
  lastModDate: Date,                // a Date
  isDirectory: boolean,             // True if directory
  encrypted: boolean,               // True if encrypted
  externalFileAttributes: number,   // platform specific file attributes
  versionMadeBy: number,            // platform that made this file
};

export interface Reader {
  getLength(): Promise<number>,
  read(offset: number, size: number): Promise<Uint8Array>,
}

export type UnzipitOptions = {
  useWorkers?: boolean;
  workerURL?: string;
  numWorkers?: number;
};

export type TypedArray = Int8Array | Uint8Array | Int16Array | Uint16Array | Int32Array | Uint32Array | Float32Array;

export class HTTPRangeReader implements Reader {
  constructor(url: string);
  getLength(): Promise<number>;
  read(offset: number, size: number): Promise<Uint8Array>;
}

export function unzip(src: string | ArrayBuffer | TypedArray | Blob | Reader): Promise<ZipInfo>;
export function unzipRaw(src: string | ArrayBuffer | TypedArray | Blob | Reader): Promise<ZipInfoRaw>;
export function setOptions(options: UnzipitOptions): void;
export function cleanup(): void;
