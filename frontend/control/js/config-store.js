import { api } from './api.js';
import { migrateConfig } from './config-migrate.js';

// the loaded pages' editors, by id; the zone dialog reads them too
export const editors = new Map();
const anyDirty = () => [...editors.values()].some(m => m.dirty);

// edits typed into a field go after a pause; a save sends the waiting ones first
const pendingDebounces = new Set();
export function debounce(fn, ms) {
    let t, args;
    const flush = () => { clearTimeout(t); pendingDebounces.delete(flush); fn(...args); };
    const wrapped = (...a) => { args = a; clearTimeout(t); pendingDebounces.add(flush); t = setTimeout(flush, ms); };
    wrapped.cancel = () => { clearTimeout(t); pendingDebounces.delete(flush); };
    return wrapped;
}
export const flushDebounces = () => { [...pendingDebounces].forEach(f => f()); };

export const getNested = (obj, path) => path?.split('.').reduce((a, p) => a?.[p], obj);
export function setNested(obj, path, val) {
    const keys = path.split('.'), last = keys.pop();
    keys.reduce((a, k) => (a[k] && typeof a[k] === 'object') ? a[k] : (a[k] = {}), obj)[last] = val;
}

// Shared /api/config fetch: readers reuse one request, each getting its
// own copy to mutate. Invalidated on save and on every panel open.
export const ConfigStore = {
    promise: null,
    // the configuration as last read or saved, never changed in place
    cached: null,
    fetch(fresh) {
        if (fresh || !this.promise) {
            const p = api('/api/config').then(cfg => (this.cached = migrateConfig(cfg)));
            p.catch(() => { if (this.promise === p) this.promise = null; });
            this.promise = p;
        }
        return this.promise.then(cfg => JSON.parse(JSON.stringify(cfg)));
    },
    invalidate() { this.promise = null; },
    // a fresh copy, changed by `change` and posted; nothing is written when `change` returns false
    async save(change) {
        flushDebounces();
        const cfg = await this.fetch(true);
        if (change(cfg) === false) return cfg;
        if (!cfg.config) cfg.config = 'aiscatcher';
        if (!cfg.version) cfg.version = 1;
        await api('/api/config', { body: cfg });
        this.invalidate();
        this.cached = JSON.parse(JSON.stringify(cfg));
        return cfg;
    }
};

export const App = {
    state: { unsaved: false },

    // the app shows these in its message bar once it has one
    notify(type, message) {
        console[type === 'error' ? 'error' : 'log'](message);
        return () => {};
    },

    setUnsaved(bool) {
        if (bool && this.state.unsaved) return;
        if (!bool) editors.forEach(m => { m.dirty = false; });
        this.state.unsaved = bool;
        const footer = document.getElementById('st-footer');
        if (footer) {
            footer.querySelector('.st-dirty').hidden = !bool;
            footer.querySelector('[data-st-save]').disabled = !bool;
        }
        window.onbeforeunload = bool ? () => true : null;
    },

    // at most one tab can hold unsaved edits, but a tab may span several editors
    async saveDirty() {
        flushDebounces();
        for (const m of [...editors.values()])
            if (m.dirty) await m.save();
    }
};

// The page marks its edits with `touched`, writes them with `write` (or `clean` when they went some
// other way) and leaves with `destroy`.
export function registerEditor(id, { save, data }) {
    const editor = {
        dirty: false,
        get data() { return data(); },
        save,
        touched() {
            editor.dirty = true;
            App.setUnsaved(true);
        },
        clean() {
            editor.dirty = false;
            App.setUnsaved(anyDirty());
        },
        // false, with the reason shown, when the save failed
        async write(change, what) {
            try {
                await ConfigStore.save(change);
            } catch (e) {
                App.notify('error', `Could not save ${what}: ${e.message}`);
                return false;
            }
            editor.clean();
            return true;
        },
        destroy() { if (editors.get(id) === editor) editors.delete(id); }
    };
    editors.set(id, editor);
    return editor;
}
