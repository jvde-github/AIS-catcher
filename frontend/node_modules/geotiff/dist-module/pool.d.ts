export default Pool;
/**
 * Pool for workers to decode chunks of the images.
 */
declare class Pool {
    /**
     * @constructor
     * @param {Number} [size] The size of the pool. Defaults to the number of CPUs
     *                      available. When this parameter is `null` or 0, then the
     *                      decoding will be done in the main thread.
     * @param {function(): Worker} [createWorker] A function that creates the decoder worker.
     * Defaults to a worker with all decoders that ship with geotiff.js. The `createWorker()`
     * function is expected to return a `Worker` compatible with Web Workers. For code that
     * runs in Node, [web-worker](https://www.npmjs.com/package/web-worker) is a good choice.
     *
     * A worker that uses a custom lzw decoder would look like this `my-custom-worker.js` file:
     * ```js
     * import { addDecoder, getDecoder } from 'geotiff';
     * addDecoder(5, () => import ('./my-custom-lzw').then((m) => m.default));
     * self.addEventListener('message', async (e) => {
     *   const { id, fileDirectory, buffer } = e.data;
     *   const decoder = await getDecoder(fileDirectory);
     *   const decoded = await decoder.decode(fileDirectory, buffer);
     *   self.postMessage({ decoded, id }, [decoded]);
     * });
     * ```
     * The way the above code is built into a worker by the `createWorker()` function
     * depends on the used bundler. For most bundlers, something like this will work:
     * ```js
     * function createWorker() {
     *   return new Worker(new URL('./my-custom-worker.js', import.meta.url));
     * }
     * ```
     */
    constructor(size?: number, createWorker?: () => Worker);
    workerWrappers: Promise<WorkerWrapper[]> | null;
    /**
     * @param {number} compression
     * @param {import('./compression/basedecoder.js').BaseDecoderParameters} decoderParameters
     * @returns {import('./geotiff.js').DecoderWorker}
     */
    bindParameters(compression: number, decoderParameters: import("./compression/basedecoder.js").BaseDecoderParameters): import("./geotiff.js").DecoderWorker;
    destroy(): Promise<void>;
}
/**
 * @module pool
 */
/**
 * Wrapper for a worker that can submit jobs to the worker and receive responses.
 */
declare class WorkerWrapper {
    /**
     * @param {Worker} worker the worker to wrap
     */
    constructor(worker: Worker);
    /** @type {Worker} */
    worker: Worker;
    jobIdCounter: number;
    jobs: Map<any, any>;
    /**
     * Get a new job id
     * @returns {Number} the new job id
     */
    newJobId(): number;
    /**
     * Get the number of jobs currently running
     * @returns {Number} the number of jobs currently running
     */
    getJobCount(): number;
    /** @param {MessageEvent} e */
    _onWorkerMessage(e: MessageEvent): void;
    /**
     * Submit a job to the worker
     * @param {Record<string, unknown>} message the message to send to the worker. A "jobId" property will be added to this object.
     * @param {Array<Transferable>} [transferables] an optional array of transferable objects to transfer to the worker.
     * @returns {Promise<{decoded: ArrayBuffer}>} a promise that gets resolved/rejected when a message with the same jobId is
     * received from the worker.
     */
    submitJob(message: Record<string, unknown>, transferables?: Array<Transferable>): Promise<{
        decoded: ArrayBuffer;
    }>;
    terminate(): void;
}
//# sourceMappingURL=pool.d.ts.map