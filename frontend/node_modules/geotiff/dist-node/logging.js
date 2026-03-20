"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.setLogger = setLogger;
exports.debug = debug;
exports.log = log;
exports.info = info;
exports.warn = warn;
exports.error = error;
exports.time = time;
exports.timeEnd = timeEnd;
/**
 * A no-op logger
 */
class DummyLogger {
    /** @param {...unknown} _args */
    log(..._args) { }
    /** @param {...unknown} _args */
    debug(..._args) { }
    /** @param {...unknown} _args */
    info(..._args) { }
    /** @param {...unknown} _args */
    warn(..._args) { }
    /** @param {...unknown} _args */
    error(..._args) { }
    /** @param {...unknown} _args */
    time(..._args) { }
    /** @param {...unknown} _args */
    timeEnd(..._args) { }
}
let LOGGER = new DummyLogger();
/**
 * @param {DummyLogger} logger the new logger. e.g `console`
 */
function setLogger(logger = new DummyLogger()) {
    LOGGER = logger;
}
/** @param {...unknown} args */
function debug(...args) {
    return LOGGER.debug(...args);
}
/** @param {...unknown} args */
function log(...args) {
    return LOGGER.log(...args);
}
/** @param {...unknown} args */
function info(...args) {
    return LOGGER.info(...args);
}
/** @param {...unknown} args */
function warn(...args) {
    return LOGGER.warn(...args);
}
/** @param {...unknown} args */
function error(...args) {
    return LOGGER.error(...args);
}
/** @param {...unknown} args */
function time(...args) {
    return LOGGER.time(...args);
}
/** @param {...unknown} args */
function timeEnd(...args) {
    return LOGGER.timeEnd(...args);
}
//# sourceMappingURL=logging.js.map
//# sourceMappingURL=logging.js.map