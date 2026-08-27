/* ============================================================================
   settings.js — a persisted settings object (layer 2)

   An ES module, like components.js.

   What a host gets: a plain object it may read and write, kept in localStorage
   under a key it chooses, merged over its own defaults on load, with a hook for
   renaming keys between versions.

   What stays with the host: which keys exist, what they mean, when to save, and
   any state it wants to harvest before saving (a card's open rows, the active
   receiver). This holds an object; it does not know what is in it.

   var store = create({
       key: "aiscatcher:" + stationId,   // namespaced: one origin, many stations
       defaults: { dark_mode: false, metric: "DEFAULT" },
       version: 2,                       // optional: an entry saved by an older version is discarded
       migrate: function (s) { ... },    // optional, runs once after load
   });
   store.set("dark_mode", true);         // writes and saves
   store.all().metric;                   // the live object
   ========================================================================= */

export function create(opts) {
    if (!opts || !opts.key) throw new Error("settings.create needs a key");

    var key = opts.key;
    /* a host with an existing state object hands it over as `target`: the
       store fills and persists that object, so bindings elsewhere hold */
    var values = opts.target || {};
    var listeners = [];
    var defaults = opts.defaults || {};
    var version = opts.version || 0;
    var upgraded = false;

    for (var d in defaults) if (Object.prototype.hasOwnProperty.call(defaults, d)) values[d] = JSON.parse(JSON.stringify(defaults[d]));

    /* A corrupt or foreign entry must not take the page down with it: the
       defaults are a working configuration, so fall back to them and say so. */
    function load(skip) {
        if (!skip) try {
            var raw = localStorage.getItem(key);
            if (raw !== null) {
                var parsed = JSON.parse(raw);
                if (parsed && typeof parsed === "object") {
                    /* an entry without a version is from before versions existed: 1 */
                    upgraded = version > 0 && (parsed.settings_version || 1) < version;
                    if (!upgraded)
                        for (var k in parsed) if (Object.prototype.hasOwnProperty.call(parsed, k)) values[k] = parsed[k];
                }
            }
        } catch (e) {
            console.warn("settings: ignoring unreadable entry for " + key, e);
        }
        if (version) values.settings_version = version;
        if (opts.migrate) opts.migrate(values);
        return values;
    }

    function applyDefaults() {
        for (var k in values) if (Object.prototype.hasOwnProperty.call(values, k)) delete values[k];
        for (var d in defaults) if (Object.prototype.hasOwnProperty.call(defaults, d)) values[d] = JSON.parse(JSON.stringify(defaults[d]));
        if (version) values.settings_version = version;
    }

    function save() {
        try {
            localStorage.setItem(key, JSON.stringify(values));
        } catch (e) {
            /* a full or blocked store must not break the interaction that
               triggered the write; the value stays live for this session */
            console.warn("settings: could not persist " + key, e);
        }
        for (var i = 0; i < listeners.length; i++) listeners[i](values);
    }

    return {
        all: function () { return values; },
        get: function (k) { return values[k]; },
        set: function (k, v) { values[k] = v; save(); },
        stage: function (k, v) { values[k] = v; },   // live, unsaved: a slider mid-drag
        assign: function (patch) {
            for (var k in patch) if (Object.prototype.hasOwnProperty.call(patch, k)) values[k] = patch[k];
            save();
        },
        save: save,
        load: load,
        upgraded: function () { return upgraded; },   // the last load discarded an older entry
        /* in place: other modules hold a reference to this object.
           applyDefaults does not persist — startup fills before it loads,
           and writing there would erase what it is about to read. */
        applyDefaults: applyDefaults,
        reset: function () { applyDefaults(); save(); },
        onChange: function (fn) { listeners.push(fn); },
        key: key,
    };
}

