# The frontend

Three packages and two apps, built by one Vite configuration and baked into the
binary by `scripts/build-html.sh`.

```
packages/core   pure: ship classes and types, units, geometry, filters, sprites.
                No DOM, no map library. A worker may import it.
packages/ui     the DOM: tokens and stylesheets (css/), components, cards,
                panels, tables, search, ticker.
packages/map    what draws on an OpenLayers map: markers, hulls and tracks,
                map chrome, map objects, measuring, ports.
src/            the viewer. script.js wires it; a feature or a tab is a folder
                with its code, its markup and its stylesheet (features/, tabs/).
control/        the hub (managed mode): control-app.js bundles control/js.
test/           the packages' tests (node --test) and the smoke test.
```

**The rule**: `core` ← `ui` ← `map` ← the apps. A package imports the ones
before it, never after, and `core` never touches `window` or `document`.
`eslint.config.js` states this as an error, so a crossing fails the lint.

**The page** is `src/index.html`, the shell; where it says
`<!-- @include tabs/x/x.html -->` the build puts that feature's markup.
`src/app.css` imports the stylesheets in cascade order and Vite bundles them
into `lib.css`. The hub keeps its own concatenation of the first `ui` sheets.

**Actions**: markup names one with `data-action="name"` (a click), or
`data-on-change`, `data-on-input`, `data-on-contextmenu`; `src/actions.js`
dispatches them, and each module registers the ones it owns with
`register({ name: handler })` - a lazy tab when it loads. Plugins see the
same registry as `AISCatcher.ACTIONS`.

**Tabs**: one table in `src/tabs/index.js`: each tab's menu entry, the server
feature it needs, its module, loaded once, and what shows, hides and refreshes
it. `showTab(id)` is the one way to a tab; a lazy tab's module exports `shown()`,
`hidden()` and `update()`.

**Cards**: `src/features/targetcard/card.js` is the shell, with the one rule of
the tab and the fold; ship, plane and station are providers of one shape
`{ build, populate, open }`, picked by type.

**Shared things have one owner, and features import it**: `src/ui.js` is the map's
UI, `src/settings-store.js` the settings. Import cycles between features are
fine as long as a module's top level asks nothing of them.

**Settings** in the viewer: a control that holds a setting carries
`data-setting="key"`; the panel (`packages/ui/panel.js`) keeps it and the
setting in step, and `SETTING_EFFECTS` in `src/settings-store.js` names what
else the change means: each owner registers its own with `effects()`, beside
its actions. All of them run after the settings load and after a reset to the
defaults. A control whose change is more than a setting keeps a `data-on-change`
action.

**Working on it**

    npm install                      # once, at frontend/; a workspace
    npm run lint                     # the direction rule included
    npm test                         # the packages' tests
    ../scripts/build-html.sh         # builds both apps and bakes them
    ../scripts/smoke.sh              # starts a viewer and a hub from the binary and drives both

Dependencies are pinned. `@aiscatcher/core`, `ui` and `map` carry manifests
for publishing; the website takes them from the registry.
