import type { CodecConstructor } from './types';
interface LZ4Config {
    acceleration?: number;
}
declare const LZ4: CodecConstructor<LZ4Config>;
export default LZ4;
