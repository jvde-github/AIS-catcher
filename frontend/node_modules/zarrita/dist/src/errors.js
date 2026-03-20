export class NodeNotFoundError extends Error {
    constructor(context, options = {}) {
        super(`Node not found: ${context}`, options);
        this.name = "NodeNotFoundError";
    }
}
export class KeyError extends Error {
    constructor(path) {
        super(`Missing key: ${path}`);
        this.name = "KeyError";
    }
}
//# sourceMappingURL=errors.js.map