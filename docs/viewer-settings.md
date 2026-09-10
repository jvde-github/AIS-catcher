# Live viewer settings

Managed Control saves the normal `control.viewer` JSON. The viewer owner checks
for changes while idle and while the receiver runs. It prepares the desired
settings first, then applies them under its state lock, synchronizing with AIS
input processing before changing tracking configuration or stream connections.
Invalid viewer settings are rejected by Control before the configuration is saved.
The HTTP listener, ships and statistics remain available; map updates do not use
the old stop/reset/start cycle.

The backend has one configuration pipeline:

1. `ViewerConfiguration` holds requested values and prepared resources. Its
   `SetKey()` is shared by CLI parsing, JSON loading and validation. It does not
   modify a live database, listener, log subscription or backup worker. Managed
   startup and live saves also share `prepareManagedConfig()`; no temporary
   WebViewer is constructed to validate or prepare settings.
2. `ViewerSettings.cpp` owns `applySettings()`. Startup, live updates, receiver
   attachment and catalogue refresh all use this entry point. It resolves restart
   restrictions, computes `SettingsChanges`, initializes only fresh trackers,
   applies the effects and publishes effective frontend features.
3. Requested values remain separate from effective values. A deferred storage
   change keeps its restart notice through unrelated edits and receiver restarts;
   it cannot silently disappear because an application overwrote the request.

The application modes express lifecycle permissions: Start activates services,
Live retains receiver subscriptions, and Attach accepts the subscriptions just
wired by the engine. Database allocation and backup restore happen only once;
reused trackers are never initialized again. Stop only suspends workers and log
subscriptions. It does not reset the configuration comparison baseline.

To add a setting, put its value/default and parser in `ViewerConfiguration`, then
add any resource comparison to `SettingsChanges` and its effect to `applySettings`.
Expose browser-facing values in `FrontendConfig` where necessary. Do not put
runtime effects in `SetKey`, `startServing`, `endAttach` or the managed save path.
The API lifecycle regression compares live settings, receiver restart and service
restart, including retained/restored vessels and pending storage changes.

`/viewer/api/config.json` (or `/api/config.json` on a standalone viewer) returns
only browser-facing settings, not the Control configuration or credentials.
`config_version` is also included at the top level of `api/ships_array.json`.
The managed sharing-state heartbeat also announces it, so updates reach other
tabs and replay mode even when vessel polling is paused. The version is a content digest: saving identical settings does not invalidate
browser caches. The browser fetches the small complete configuration document
when its version changes and applies differences in place. Failed and stale
responses are retried on a later vessel poll.

The bootstrap module fetches this JSON before initializing controls, with the
embedded configuration as a fallback. The frontend updater handles tab visibility,
station title, receiver choices, about content and server map sources. Map-source
settings are JSON descriptors, rather than generated plugin code. Unchanged
OpenLayers sources are retained, and existing map position and preferences survive
settings updates. Place geometry keeps its own `place_version` and fetch path;
changing an area's boundary does not download unrelated settings or reload the page.

`plugin_version` identifies the JavaScript and stylesheet content loaded by the
page. Adding, removing or changing plugins produces a Reload prompt. Plugins are
not executed a second time in the current page. Reload saves viewer preferences,
map position and the selected ship. Saving configuration re-reads plugins, so a
plugin file edit can be applied by saving the configuration; there is no filesystem
watcher. Place files edited outside Control still require reloading the directory
or restarting, as before.

Tracking storage sizing requires a service restart; receiver subscription/split
changes require a receiver restart. Both are reported in `restart_required` and
retain their effective values in the meantime. Stopping and starting reception
does not discard the existing tracking database to resize it. Managed listener binding continues to belong to Control; this update
path does not rebind its socket. Other engine settings still use the normal receiver
restart workflow. A browser reload for plugins is separate from a receiver restart.

Validation:

- `node --test frontend/shared/test/server-config.test.js`: unchanged versions,
  failed requests and superseded in-flight replies.

Station position changes (configuration, GPS or own-vessel reports) recalculate
cached distances and bearings for existing vessels once the station has moved
about 100 m from the position they were computed for; GPS jitter below that
changes the reported station position only. Missing coordinates clear those
values. The compact feed marks a forced refresh with `full: true`; clients then
replace their cached fleet rather than merge it. Place catalogue changes explicitly
rematch existing ships and establish fresh crossing baselines. Database reads no
longer poll an area provider. Backup restores also recompute derived fields.

GeoJSON and KML capabilities are versioned with the frontend settings and their
routes switch immediately. Disabling realtime also closes background streams.
Backup interval changes reschedule the worker; zero disables periodic writes.
An empty filename disables saving, including shutdown. A configured file is read
once during startup; changing its name while running redirects future writes and
never restores over live state. Shutdown still saves a nonempty filename.
