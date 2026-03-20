/**
 * Parse a 'Content-Type' header value to the content-type and parameters
 * @param {string|undefined} rawContentType the raw string to parse from
 * @returns {{type: string|null, params: Record<string, string>}}
 *     the parsed content type with the fields: type and params
 */
export function parseContentType(rawContentType: string | undefined): {
    type: string | null;
    params: Record<string, string>;
};
/**
 * Parse a 'Content-Range' header value to its start, end, and total parts
 * @param {string|undefined} rawContentRange the raw string to parse from
 * @returns {{start: number, end: number, total: number}} the parsed parts
 */
export function parseContentRange(rawContentRange: string | undefined): {
    start: number;
    end: number;
    total: number;
};
/**
 * Parses a list of byteranges from the given 'multipart/byteranges' HTTP response.
 * Each item in the list has the following properties:
 * - headers: the HTTP headers
 * - data: the sliced ArrayBuffer for that specific part
 * - offset: the offset of the byterange within its originating file
 * - length: the length of the byterange
 * @param {ArrayBuffer} responseArrayBuffer the response to be parsed and split
 * @param {string} boundary the boundary string used to split the sections
 * @returns {Array<{headers: Record<string, string>, data: ArrayBuffer, offset: number, length: number, fileSize: number}>}
 *     the parsed byteranges
 */
export function parseByteRanges(responseArrayBuffer: ArrayBuffer, boundary: string): Array<{
    headers: Record<string, string>;
    data: ArrayBuffer;
    offset: number;
    length: number;
    fileSize: number;
}>;
//# sourceMappingURL=httputils.d.ts.map