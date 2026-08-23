# Shared frontend

The look and the shells of AIS-catcher's frontends, for any host that wants
them: the viewer (`frontend/src`), the control hub (`frontend/control`), the
community website. Hosts vendor a commit of this directory and never edit
their copy; `CHANGELOG.md` lists what changed between two commits,
`index.js` exports `VERSION`.

## What is here

| file | what | shape |
|---|---|---|
| `tokens.css` | design tokens, light and dark | the `dark` class on `<html>` switches |
| `icons.css` | icon classes (`name_icon`), `currentColor` masks | |
| `components.css` / `components.js` | buttons, inputs, tabs, dialog, toast, field rows, flag, chip | free functions |
| `sprites.css` | the ship-class chip sheets `core/filter.js` points into | |
| `map.css` | map chrome: pills, toolbar, ticker, cards; registers `--map-inset-*` | |
| `card.js` | card shell: sections, paged footer, popovers | `create({ mount })` |
| `chrome.js` | where a floating panel may sit | `create({ obstacles, fallback })` + pure geometry |
| `settings.js` | a persisted settings object | `create({ key, defaults })` |
| `theme.js` | sets the palette, can follow the system | `apply("dark" \| "light" \| "system")` |
| `markers.js` | sprite registry, hull/range geometry, OpenLayers styles for markers, hulls, tracks, labels, rings | `create({ settings, hover, selected, opacity })` + pure registry; needs `ol` |
| `mapui.js` | all of the map UI from one `create()` | `create({ map, settings, card, toolbar, menu, markers })` |
| `ticker.js` | event strip: slots, chips, cadence, panning; host pushes described events | `create({ mount, buckets, onSelect })` |
| `toolbar.js` | the map buttons as a rail or a labelled bar | `create({ bar, rail, wantWide, freeWidth })` |
| `menu.js` | context menu: items, tags, checks, separators, placement, dismissal | `create({ mount, checks })` |
| `panel.js` | the settings window: group tabs, sub-tabs, pages | `create({ window, groups, onOpen })` |
| `color.js` | colour parsing and label backgrounds | pure |
| `core/units.js` | distance/speed/dimension/coordinate formatting | `create({ system, coordinates })` |
| `core/text.js` | names for statuses, types, groups, times | pure |
| `core/spark.js` | inline-SVG charts for cards | pure, take a units instance |
| `core/filter.js` | what a vessel filter means | `create({ state, now })` + pure buckets |
| `core/geo.js`, `core/constants.js` | geodesic math, AIS enums | pure |

`index.js` re-exports all of it; `index.css` imports the stylesheets in order —
a host that cannot `@import` concatenates them in that order into one file
(AIS-catcher's build does this: `shared.css`).
`npm test` runs the `core/` tests with `node --test`.

## Conventions

- **Stateless things are functions; stateful things are factories.** Nothing
  is a module singleton, so a page can hold two maps, two filters, two unit
  systems.
- **A component is given what it needs.** No import of a store, a settings
  object or another component's DOM. `core/` never touches the DOM at all.
- **How results come back.** Geometry *returns* values (`chrome`). Primitives
  take *callbacks* (`toast`, `modal`, `dismissOnce`). Shells *dispatch
  events on their mount* (`card:section`). Classic-script hosts can listen to
  all three.
- **Colour by class, never by attribute or inline style.** Every colour is a
  `var(--token)`; SVG parts carry classes (`.dim-arrowhead`, `.tl-dot`), so
  dark mode needs no JavaScript and a host can run a strict CSP.
- **Markup opts in by class.** Shared CSS names no host ids. The settings
  panel's plain markup is tagged by `panel.adoptClasses()` rather than having
  `components.css` know its selectors.
- **Sizes are asked for, not assumed.** `chrome.freeBox()` reads the insets
  a host declares in CSS (`--map-inset-top` …); nothing hard-codes a ticker
  or a panel.

## Map UI in one call

`mapui.create({ map, settings, card, toolbar, menu, markers })` wires chrome,
markers, card shell, toolbar, context menu and view helper with defaults for
a page laid out with `map.css`. A host hands over its elements and overrides
only what is particular to it:

```js
const ui = mapui.create({
  map: () => map, settings: () => settings,
  card: { mount: cardEl }, toolbar: { bar, rail }, menu: { mount: menuEl, checks },
  markers: { hover: () => hover, selected: () => selected, lookup },
});
new VectorLayer({ source, style: ui.markers.marker });
ui.showCard(pixel, [lon, lat]);       // place, fit, keep the target on screen
ui.reveal([lon, lat]);                // after the card grew: pan if it now covers the target
ui.layout();                          // on resize / panel change
```

## Using it in a host

Vendor, then link or bundle:

```html
<link rel="stylesheet" href="shared/index.css">
<script type="module">
  import { card, chrome, units, theme, components } from "./shared/index.js";
  theme.apply("system");
</script>
```

Build a card:

```js
const c = card.create({ mount: document.getElementById("shipcard") });
const cells = c.rows(null, [
  { section: { key: "voyage", label: "Voyage", open: true },
    fields: [{ key: "dest", label: "Destination" }, { key: "eta", label: "ETA" }] },
], { group: "ship" });
cells.dest.textContent = "ROTTERDAM";
c.footer.add({ icon: "route", label: "Track", title: "Show track", action: "showTrack", group: "ship" });
c.footer.show("ship");
```

The markup a card needs: an element with `mapcard mapcard-target card-width
card-max`, containing `.mapcard-header` (with `.mapcard-flag`,
`.mapcard-title`), `.mapcard-content` and `.mapcard-footer`. Sections and
rows are built by `rows()`, or written by hand with `data-section` attributes.

External dependency: `components.flag()` emits `fi fi-xx` classes and expects
the host to load the `flag-icons` stylesheet; without it the span is empty.

A classic-script host (no bundler, no modules) loads one module file that
publishes what it needs on `window` — see `control/js/shared-globals.js`.
