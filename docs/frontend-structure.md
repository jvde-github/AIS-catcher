# Frontend ownership

The viewer starts in `frontend/src/bootstrap.js`. It loads current server
settings through `features/server-config.js` before importing the viewer.
That module owns the configuration request, difference detection, publication
and feature visibility. `script.js` supplies effects on the active tabs, title,
selection and map. `features/server-maps.js` manages configured tile layers.

`features/mapobjects.js` adapts viewer transport and settings to
`frontend/shared/mapobjects.js`, which owns object rendering and interaction.
Binary-message settings retain their existing names; the object layer and
transport use `objectLayer` and `fetchObjects`.

Managed control starts in the bundled `control-app.js`, whose source is
`frontend/control/js/app.js`. It directly imports `createPlaceEditor` from `places-editor.js` and owns its
instance. Places is a top-level tab without a redundant sub-tab. The shared
`places.js` helper converts summary rows into editable GeoJSON metadata. Their CSS lives in
`frontend/control/css/locations.css`, after the base control styles.

Vite keeps shared configuration and its preload helper outside entry chunks.
Importing shared code must not import and execute a bootstrap entry point.
The control bundle reuses OpenLayers; copied editor source files are excluded
from the baked assets.

`script.js` still contains substantial legacy UI and map orchestration. Extract
those by responsibility as they change; do not introduce a general framework
or move everything behind one opaque controller.
