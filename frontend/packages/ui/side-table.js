// A context occupies the existing table panel without replacing its overview.
// Each host owns opening the panel and updating its map layout.

// Match panel.css's workspace query: when the table covers the map, opening
// either surface dismisses the other through the host's normal close action.
// If a resize removes room for both, keep the one the user opened last.
export function createPanelSwitch({ workspace, card, table }) {
    const panels = { card, table };
    let latest = 'table';
    const fullWidth = () => (workspace?.clientWidth || window.innerWidth) <= 750;
    const dismissOther = () => {
        const other = panels[latest === 'card' ? 'table' : 'card'];
        if (fullWidth() && other.isOpen()) other.close();
    };
    const resize = new ResizeObserver(() => {
        if (card.isOpen() && table.isOpen()) dismissOther();
    });
    if (workspace) resize.observe(workspace);
    return {
        opening(panel) {
            latest = panel;
            dismissOther();
        },
        destroy: () => resize.disconnect(),
    };
}

export function createSideTable(host, id) {
    let panel = document.getElementById('tableside');
    if (!panel) {
        panel = document.createElement('aside');
        panel.id = 'tableside';
        panel.className = 'tableside_window';
        (document.querySelector('.mainspace-container') || document.body).appendChild(panel);
    }
    let root = document.getElementById(id);
    if (!root) {
        root = document.createElement('section');
        root.id = id;
        root.className = 'side-table-context hidden';
        panel.appendChild(root);
    }
    root._dispose?.();

    const setOpen = on => {
        if (host.setTableOpen) host.setTableOpen(on);
        else {
            panel.classList.toggle('active', on);
            document.body.classList.toggle('table-open', on);
            host.map?.()?.updateSize();
        }
    };
    const leave = open => {
        root._dispose?.();
        root.classList.add('hidden');
        if (panel.dataset.context !== id) return;
        delete panel.dataset.context;
        setOpen(open);
        if (open) panel.dispatchEvent(new CustomEvent('table:overview'));
    };
    return {
        root, panel,
        open() {
            for (const other of panel.querySelectorAll('.side-table-context')) {
                if (other === root) continue;
                other._dispose?.();
                other.classList.add('hidden');
            }
            panel.dataset.context = id;
            root.classList.remove('hidden');
            setOpen(true);
        },
        close: () => leave(false),
        overview: () => leave(true),
        isOpen: () => panel.classList.contains('active') && panel.dataset.context === id && !root.classList.contains('hidden'),
    };
}
