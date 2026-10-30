// Every request of the panel to the control server, read as JSON. `body`: an object goes as JSON,
// a string as it is, and either makes it a POST. A 401 asks for the password again and leaves the
// edits on the page, unless `login` (the login's own request, whose 401 is a wrong password). A
// failure rejects with the server's reason. `raw`: the response as it came, a 401 included (the
// API tester's, from the server it tries).
import { hub } from './hub.js';

export async function api(path, { body, method, login = false, raw = false, headers, ...init } = {}) {
    const json = body !== undefined && typeof body !== 'string';
    let r;
    try {
        r = await fetch(path, {
            ...init,
            method: method || (body !== undefined ? 'POST' : 'GET'),
            headers: json ? { 'Content-Type': 'application/json', ...headers } : headers,
            body: json ? JSON.stringify(body, null, 2) : body
        });
    } catch {
        throw new Error('Connection error. Please try again.');
    }
    if (raw) return r;
    if (r.status === 401 && !login) hub.authRequired();
    const data = await r.json().catch(() => undefined);
    if (!r.ok || data?.status === false) throw new Error(data?.error || `the server answered ${r.status}`);
    if (data === undefined) throw new Error('the server answered with no JSON');
    return data;
}
