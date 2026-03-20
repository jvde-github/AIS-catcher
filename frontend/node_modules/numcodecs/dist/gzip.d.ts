import type { CodecConstructor } from './types';
interface GZipConfig {
    level?: number;
}
declare const GZip: CodecConstructor<GZipConfig>;
export default GZip;
