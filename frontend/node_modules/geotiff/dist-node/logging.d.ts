/**
 * @param {DummyLogger} logger the new logger. e.g `console`
 */
export function setLogger(logger?: DummyLogger): void;
/** @param {...unknown} args */
export function debug(...args: unknown[]): void;
/** @param {...unknown} args */
export function log(...args: unknown[]): void;
/** @param {...unknown} args */
export function info(...args: unknown[]): void;
/** @param {...unknown} args */
export function warn(...args: unknown[]): void;
/** @param {...unknown} args */
export function error(...args: unknown[]): void;
/** @param {...unknown} args */
export function time(...args: unknown[]): void;
/** @param {...unknown} args */
export function timeEnd(...args: unknown[]): void;
/**
 * A no-op logger
 */
declare class DummyLogger {
    /** @param {...unknown} _args */
    log(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    debug(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    info(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    warn(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    error(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    time(..._args: unknown[]): void;
    /** @param {...unknown} _args */
    timeEnd(..._args: unknown[]): void;
}
export {};
//# sourceMappingURL=logging.d.ts.map