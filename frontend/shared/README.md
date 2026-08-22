# Shared frontend

Everything in here is consumed by more than one host: the viewer (`frontend/src`),
the control hub (`frontend/control`), and the community website. The build copies
these files out; hosts never edit their copy.

Four rules keep a component portable. A file that breaks one of them belongs in a
host, not here.

1. No host state. Nothing imports a store, a settings object, or another
   component's DOM. What a component needs, it is given.
2. No literal colours or sizes in CSS. Only `var(--token)`; hosts supply values.
3. One entry shape: `init({ mount, data, format, on, chrome })` — `data` holds the
   async adapters, so swapping endpoints is all a new host has to do.
4. Space is asked for, not assumed: `chrome.availableBox()`, never a hard-coded
   inset for a ticker or a panel that may not exist.

Currently here: design tokens, primitives (buttons, inputs, tabs, dialog, toast),
icons, and placement — one free box the host describes in CSS, plus the placers
that work off it, so a tooltip and a panel cannot disagree about where the map
ends. Planned: context menu, target card, ticker, the ship table, and the
settings panel shell once it is separated from what the viewer puts inside it.
