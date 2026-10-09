// One delayed indicator per surface; an older request cannot clear a newer one.
const pending = new WeakMap();

export function beginLoading(target, label = 'Loading', mount = target) {
    pending.get(target)?.();
    let indicator;
    const timer = setTimeout(() => {
        indicator = document.createElement('span');
        indicator.className = 'loading-indicator spinner icon-sm';
        indicator.setAttribute('role', 'status');
        indicator.setAttribute('aria-label', label);
        mount.appendChild(indicator);
        target.classList.add('is-loading');
    }, 400);
    const stop = () => {
        clearTimeout(timer);
        indicator?.remove();
        if (pending.get(target) !== stop) return;
        pending.delete(target);
        target.classList.remove('is-loading');
        target.setAttribute('aria-busy', 'false');
    };
    pending.set(target, stop);
    target.setAttribute('aria-busy', 'true');
    return stop;
}
