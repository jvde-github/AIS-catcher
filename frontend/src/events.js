const actions = {};
const contextActions = {};
const changes = {};
const inputs = {};

export function registerActions(map)        { Object.assign(actions, map); }
export function registerContextActions(map) { Object.assign(contextActions, map); }
export function registerChanges(map)        { Object.assign(changes, map); }
export function registerInputs(map)         { Object.assign(inputs, map); }

export function initEvents(activateTab) {
    // Capture phase so stopPropagation() inside OL/library handlers doesn't block us.
    document.addEventListener('click', e => {
        const el = e.target.closest('[data-action],[data-tab]');
        if (!el) return;
        if (el.dataset.tab !== undefined) { activateTab(e, el.dataset.tab); return; }
        actions[el.dataset.action]?.call(el, e);
    }, true);

    document.addEventListener('contextmenu', e => {
        const el = e.target.closest('[data-contextaction]');
        if (el) contextActions[el.dataset.contextaction]?.call(el, e);
    }, true);

    document.addEventListener('change', e => {
        const el = e.target;
        if (!el.dataset.change) return;
        const fn = changes[el.dataset.change];
        if (!fn) return;
        const val = el.type === 'checkbox' ? el.checked : el.value;
        el.dataset.arg !== undefined ? fn(el.dataset.arg, val) : fn(val);
    }, true);

    document.addEventListener('input', e => {
        const el = e.target;
        if (el.dataset.input) inputs[el.dataset.input]?.(el.value);
    }, true);
}
