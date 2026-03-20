import type { CodecConstructor } from './types';
interface ZlibConfig {
    level?: number;
}
declare const Zlib: CodecConstructor<ZlibConfig>;
export default Zlib;
