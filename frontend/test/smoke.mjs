// Smoke test of the built pages in a headless Chrome: the viewer and the hub
// load, every tab opens, the settings panel opens, and nothing throws or fails
// to load on the way. It needs a running binary; scripts/smoke.sh starts one.
//
//     node test/smoke.mjs http://127.0.0.1:8125/ http://127.0.0.1:8140/
//
// CHROME names the browser (default: Chrome on macOS, google-chrome elsewhere).
// A request the server answers with 4xx or 5xx fails the test unless it is an
// API call, which a page without data or a login may legitimately get.
import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import WebSocket from 'ws';

const [viewerUrl, hubUrl] = process.argv.slice(2);
if (!viewerUrl) { console.error('usage: node test/smoke.mjs <viewer url> [hub url]'); process.exit(2); }
const chrome = process.env.CHROME || (process.platform === 'darwin' ? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' : 'google-chrome');
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

/* one browser per page: the simplest way to a clean profile and a clean exit */
async function open(url) {
    const dir = mkdtempSync(join(tmpdir(), 'aiscatcher-smoke-'));
    const proc = spawn(chrome, ['--headless=new', '--disable-gpu', '--hide-scrollbars', '--remote-debugging-port=0',
        '--user-data-dir=' + dir, '--window-size=1300,850', url], { stdio: 'ignore' });
    let port = null;
    for (let i = 0; i < 100 && !port; i++) {
        await wait(100);
        try { port = Number(readFileSync(join(dir, 'DevToolsActivePort'), 'utf8').split('\n')[0]); } catch { /* not yet */ }
    }
    if (!port) throw new Error('Chrome did not start: ' + chrome);
    let target;
    for (let i = 0; i < 50 && !target; i++) {
        await wait(100);
        try { target = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find((t) => t.type === 'page'); } catch { /* not yet */ }
    }
    const ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((r) => ws.on('open', r));
    let id = 0; const pending = new Map(); const errors = []; const failed = [];
    ws.on('message', (m) => {
        const d = JSON.parse(m);
        if (pending.has(d.id)) { pending.get(d.id)(d.result); pending.delete(d.id); }
        if (d.method === 'Runtime.exceptionThrown') errors.push((d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text).split('\n')[0]);
        if (d.method === 'Network.responseReceived' && d.params.response.status >= 400) {
            const path = d.params.response.url.replace(/^https?:\/\/[^/]+/, '');
            (path.startsWith('/api/') ? [] : failed).push(d.params.response.status + ' ' + path);
        }
    });
    const send = (method, params = {}) => new Promise((r) => { pending.set(++id, r); ws.send(JSON.stringify({ id, method, params })); });
    await send('Runtime.enable'); await send('Network.enable');
    const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true })).result?.value;
    const click = async (selector) => evaluate(`(() => { const el = document.querySelector(${JSON.stringify(selector)}); if (!el) return false; el.click(); return true; })()`);
    /* waits for the page to say so, up to a limit; a fixed pause is too short on a busy machine */
    const until = async (expression, ms = 20000) => { for (let i = 0; i < ms / 250; i++) { if (await evaluate(expression)) return true; await wait(250); } return false; };
    const close = async () => {
        ws.close(); proc.kill();
        await new Promise((r) => proc.once('exit', r));   // the profile is removed once Chrome has let go of it
        rmSync(dir, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
    };
    return { evaluate, click, until, errors, failed, close };
}

let failures = 0;
const check = (name, ok, detail = '') => { console.log((ok ? '  ok   ' : '  FAIL ') + name + (detail ? ': ' + detail : '')); if (!ok) failures++; };

async function viewer(url) {
    console.log('viewer ' + url);
    const page = await open(url);
    check('map drawn', await page.until("document.querySelectorAll('#map canvas').length > 0"));
    await wait(1500);
    check('page title', /AIS-catcher/.test(await page.evaluate('document.title')), await page.evaluate('document.title'));
    for (const tab of ['stat', 'plots', 'ships', 'about', 'map']) {
        const before = page.errors.length;
        const found = await page.click(`[data-action="activateTab"][data-tab="${tab}"]`);
        await wait(1500);
        check('tab ' + tab, found && page.errors.length === before, found ? page.errors.slice(before).join(' | ') : 'no such tab');
    }
    const settings = await page.click('[data-action="toggleSettings"], [data-action="openSettings"], [data-action="toggleSettingsPanel"], .settings_icon');
    await wait(1500);
    check('settings panel', settings && (await page.evaluate("document.querySelectorAll('#settings_icon_scale, .filter-check').length")) > 0);
    check('no script errors', page.errors.length === 0, page.errors.join(' | '));
    check('every asset loaded', page.failed.length === 0, page.failed.join(' | '));
    await page.close();
}

async function hub(url) {
    console.log('hub ' + url);
    const page = await open(url);
    check('hub rendered', await page.until("!!document.querySelector('#nav-btn-control')"));
    await wait(1000);
    // the system tabs live in a panel the Control button opens; they are built then, not before
    const opened = await page.click('#nav-btn-control, [data-view="control-panel"]');
    await wait(1500);
    check('control panel opens', opened && (await page.evaluate("document.querySelectorAll('.sys-pane').length")) > 0);
    for (const tab of await page.evaluate("[...document.querySelectorAll('[data-tab]')].map(e => e.dataset.tab).filter((t, i, a) => a.indexOf(t) === i)")) {
        const before = page.errors.length;
        await page.click(`[data-tab="${tab}"]`);
        await wait(800);
        check('tab ' + tab, page.errors.length === before, page.errors.slice(before).join(' | '));
    }
    check('no script errors', page.errors.length === 0, page.errors.join(' | '));
    check('every asset loaded', page.failed.length === 0, page.failed.join(' | '));
    await page.close();
}

try {
    await viewer(viewerUrl);
    if (hubUrl) await hub(hubUrl);
} catch (e) {
    console.error(e); failures++;
}
console.log(failures ? `${failures} failed` : 'all passed');
process.exit(failures ? 1 : 0);
