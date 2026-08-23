# Changelog

Breaking changes to names a host can see. Hosts vendor a commit; this is what
to read when moving between two.

## 1.0.0

First versioned surface: `index.js` / `index.css`.

- Class names use dashes throughout: `mapcard-header`, `mapcard-content`,
  `settings-window/-header/-main/-tabs/-subtabs/-tab/-nav`.
- `panel.bind(store)` / `panel.sync()` keep `data-setting` controls in step with
  a store; `components.draggable(handle, target)`; `ticker.js` is the event strip.
- `card.js` gained the layout half: `place(pixel)`, `fit()`, `pin()`/`unpin()`,
  `draggable(handle)`, `setMax()`; events `card:pin`, `card:unpin`, `card:moved`.
- New shells: `markers.js` (sprite registry + OL styles; the viewer's
  `shippingMappings`, `getSprite`, style functions and `createShipOutlineGeometry`
  now live here), `toolbar.js` (rail/bar buttons), `panel.js` (settings
  window), `menu.js` (context menu; items carry `data-tags`), `color.js` (`hexToRgb`, `deriveLabelBackground`).

- JavaScript is ES modules. The `window.AISComponents` / `AISChrome` /
  `AISSettings` globals are gone; a classic-script host publishes one itself
  (see `control/js/shared-globals.js` in AIS-catcher).
- `format.js` split into `core/units.js` (`create()` factory), `core/text.js`
  (pure), `core/spark.js` (charts; builders take a units instance). Flags are
  `components.flag()` / `flagHTML()` with `flag-card|flag-tooltip|flag-table`
  classes instead of inline styles.
- `core/filter.js`: `configure()` → `create({ state, now })`; `bucketChip`
  moved to `components.js`.
- `chrome.js`: `configure()` → `create({ obstacles, fallback })`; geometry
  (`clamp`, `alongCourse`, `beside`) stays module-level.
- `components.toast(type, message, { duration, container, onClose })`; the
  positional form still works.
- Card shell classes renamed `targetcard-*` → `card-*` (`card-section`,
  `card-max-only`, `card-max`, `card-row`, `card-popover`, `card-collapsed`,
  `card-empty`, `card-validated`, …); tokens `--color-targetcard-*` →
  `--color-card-*`. Card surfaces are styled by `.mapcard-target`,
  `.mapcard-title`, `.mapcard-flag`, not by id.
- `map.css` registers `--map-inset-*` and their defaults; `sprites.css`
  holds the chip sheets; legacy `.settings_*` aliases removed from
  `components.css`.
