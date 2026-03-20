import type { CodecConstructor } from './types';
declare enum BloscShuffle {
    NOSHUFFLE = 0,
    SHUFFLE = 1,
    BITSHUFFLE = 2,
    AUTOSHUFFLE = -1
}
interface BloscConfig {
    blocksize?: number;
    clevel?: number;
    cname?: string;
    shuffle?: BloscShuffle;
}
declare const Blosc: CodecConstructor<BloscConfig>;
export default Blosc;
