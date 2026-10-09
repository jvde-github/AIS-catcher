// The page's actions, by name. Markup names one with data-action="name" (a
// click), or data-on-change, data-on-input or data-on-contextmenu for those
// events; one delegated listener per event finds the element and calls the
// handler with (event, dataset, element). Each module registers the actions it
// owns, a lazy tab when it loads; plugins reach the same registry as
// AISCatcher.ACTIONS.

export const ACTIONS = {};

export function register(table) {
    Object.assign(ACTIONS, table);
}

// a plugin's action: a function (CSP-clean) gets a name of its own, a name
// must be registered; inline JS strings, the pre-CSP style, do not run under
// a strict CSP
let pluginActions = 0;

export function nameAction(action) {
    if (typeof action === 'function') {
        const name = '_plugin_' + ++pluginActions;
        ACTIONS[name] = action;
        return name;
    }
    if (typeof action === 'string' && ACTIONS[action]) return action;

    console.warn('addTargetcardItem: action must be a function or a registered ACTIONS key. Got:', action);
    return null;
}

function bind() {
    const handlers = [
        ['click',       '[data-action]',         'action'],
        ['change',      '[data-on-change]',      'onChange'],
        ['input',       '[data-on-input]',       'onInput'],
        ['contextmenu', '[data-on-contextmenu]', 'onContextmenu'],
    ];
    for (const [evt, sel, key] of handlers) {
        document.body.addEventListener(evt, (e) => {
            const el = e.target.closest(sel);
            if (!el) return;
            const fn = ACTIONS[el.dataset[key]];
            if (fn) fn(e, el.dataset, el);
        });
    }
}

if (document.body) bind();
else document.addEventListener('DOMContentLoaded', bind);
