export declare class NodeNotFoundError extends Error {
    constructor(context: string, options?: {
        cause?: Error;
    });
}
export declare class KeyError extends Error {
    constructor(path: string);
}
