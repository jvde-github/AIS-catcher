import type { CodecConstructor } from './types';
interface ZstdConfig {
    level?: number;
}
declare const Zstd: CodecConstructor<ZstdConfig>;
export default Zstd;
