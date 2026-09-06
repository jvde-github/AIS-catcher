// Mobile disclosure from the handle, action bar, or non-interactive content.
// Content keeps native scrolling; collapse starts only at its scroll boundary.
export function attach({ mount, panels, enabled, active, select, defaultTab = 'summary' }) {
    const handle = document.createElement('button');
    handle.type = 'button';
    handle.className = 'sc-drag-handle';
    handle.setAttribute('aria-controls', panels.id);
    const card = mount.closest('.mapcard');
    const footer = card?.querySelector('.mapcard-footer');
    // A layout rebuild replaces the content mount, but the footer survives.
    if (card) card.querySelectorAll('.sc-drag-handle').forEach(old => {
        if (old.getAttribute('aria-controls') === panels.id) old.remove();
    });
    if (mount.dataset.contextType) {
        handle.dataset.contextType = mount.dataset.contextType;
        handle.style.display = mount.style.display;
    }
    if (footer) footer.after(handle); else mount.appendChild(handle);
    let drag = null, settling = false, suppressClick = false, settleTimer;

    function update() {
        const open = active() != null;
        handle.setAttribute('aria-expanded', String(open));
        handle.setAttribute('aria-label', open ? 'Collapse card' : 'Expand Summary');
    }
    function reset() {
        panels.style.removeProperty('height');
        panels.style.removeProperty('max-height');
        panels.style.removeProperty('overflow');
        panels.style.removeProperty('transition');
    }
    function cancel() {
        clearTimeout(settleTimer);
        const current = drag;
        drag = null;
        settling = false;
        if (current && !current.touch && current.source.hasPointerCapture(current.id)) current.source.releasePointerCapture(current.id);
        reset();
    }
    function finish(cancelled) {
        if (!drag) return;
        const d = drag;
        drag = null;
        if (!d.touch && d.source.hasPointerCapture(d.id)) d.source.releasePointerCapture(d.id);
        if (!d.moved) return;
        suppressClick = true;
        const threshold = Math.min(48, d.full * 0.3);
        const target = cancelled ? d.original : d.original == null
            ? (d.dy >= threshold ? defaultTab : null)
            : (d.dy <= -threshold ? null : d.original);
        settling = true;
        const duration = window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 0 : 160;
        panels.style.transition = `height ${duration}ms ease-out`;
        panels.style.height = (target == null ? 0 : d.full) + 'px';
        settleTimer = setTimeout(() => {
            reset();
            // Restore before selecting so the host hears a genuine state change.
            select(d.original, false);
            select(target);
            update();
            settling = false;
        }, duration);
    }
    function pointerDown(e) {
        // Ship and aircraft layouts share a footer; only the visible one owns it.
        if (mount.style.display === 'none' || !enabled() || settling || drag || !e.isPrimary || e.button !== 0) return;
        suppressClick = false;
        drag = { id: e.pointerId, x: e.clientX, y: e.clientY, source: e.currentTarget,
            original: active(), moved: false, dy: 0 };
        // Keep the original button as the click target until a drag is established.
    }
    function pointerMove(e) {
        if (!drag || drag.id !== e.pointerId) return;
        drag.dy = e.clientY - drag.y;
        if (!drag.moved && Math.abs(drag.dy) < 6) return;
        if (!drag.moved) {
            if (Math.abs(e.clientX - drag.x) > Math.abs(drag.dy)) return;
            drag.moved = true;
            if (!drag.touch) drag.source.setPointerCapture(e.pointerId);
            if (drag.original == null) select(defaultTab, false);
            drag.full = panels.getBoundingClientRect().height;
            panels.style.maxHeight = 'none';
            panels.style.overflow = 'hidden';
            panels.style.transition = 'none';
        }
        const start = drag.original == null ? 0 : drag.full;
        panels.style.height = Math.max(0, Math.min(drag.full, start + drag.dy)) + 'px';
    }
    function click(e) {
        if (mount.style.display === 'none' || !enabled()) return;
        if (suppressClick && e.detail) {
            suppressClick = false;
            e.preventDefault();
            e.stopImmediatePropagation();
            return;
        }
        if (e.currentTarget !== handle || !enabled() || settling) return;
        select(active() == null ? defaultTab : null);
    }
    const listeners = {
        pointerdown: pointerDown,
        pointermove: pointerMove,
        pointerup: e => { if (drag?.id === e.pointerId) finish(false); },
        pointercancel: e => { if (drag?.id === e.pointerId) finish(true); },
        lostpointercapture: e => { if (e.target === drag?.source) finish(true); },
        click,
    };
    const surfaces = footer ? [handle, footer] : [handle];
    // Capture clicks before inline or delegated action handlers see a drag's click.
    for (const surface of surfaces)
        for (const [type, listener] of Object.entries(listeners)) surface.addEventListener(type, listener, true);
    // Touch events let iOS keep native content scrolling until we deliberately
    // claim a boundary drag. Disabling pan-y on the content would break scrolling.
    const contentListeners = {
        touchstart(e) {
            if (!drag) suppressClick = false;
            if (e.touches.length !== 1 || e.target.closest('.sc-tabs, a, button, input, select, textarea, [contenteditable], svg, canvas, .hist-wrap')) return;
            if (window.getSelection()?.toString()) return;
            const touch = e.touches[0];
            pointerDown({ isPrimary: true, button: 0, pointerId: touch.identifier,
                clientX: touch.clientX, clientY: touch.clientY, currentTarget: mount });
            if (drag?.source === mount) {
                drag.touch = true;
                drag.atEnd = panels.scrollTop + panels.clientHeight >= panels.scrollHeight - 1;
            }
        },
        touchmove(e) {
            if (!drag?.touch) return;
            if (e.touches.length !== 1) { finish(true); return; }
            const touch = e.touches[0];
            const dy = touch.clientY - drag.y;
            if (!drag.moved) {
                if (Math.max(Math.abs(dy), Math.abs(touch.clientX - drag.x)) < 6) return;
                const allowed = drag.original == null ? dy > 0 : dy < 0 && drag.atEnd;
                if (!allowed || Math.abs(touch.clientX - drag.x) > Math.abs(dy) || window.getSelection()?.toString()) {
                    drag = null; // Leave this gesture to native scrolling or selection.
                    return;
                }
            }
            if (!e.cancelable) { finish(true); return; }
            e.preventDefault();
            pointerMove({ pointerId: touch.identifier, clientX: touch.clientX, clientY: touch.clientY });
        },
        touchend(e) {
            if (!drag?.touch) return;
            if (drag.moved && e.cancelable) e.preventDefault();
            finish(false);
        },
        touchcancel() { if (drag?.touch) finish(true); },
        click,
    };
    for (const [type, listener] of Object.entries(contentListeners))
        mount.addEventListener(type, listener, { capture: true, passive: false });
    update();
    return { update, cancel, destroy() {
        cancel();
        for (const surface of surfaces)
            for (const [type, listener] of Object.entries(listeners)) surface.removeEventListener(type, listener, true);
        for (const [type, listener] of Object.entries(contentListeners)) mount.removeEventListener(type, listener, true);
        handle.remove();
    } };
}
