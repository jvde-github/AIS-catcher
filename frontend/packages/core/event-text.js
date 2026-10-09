// Shared receiver/website event contract:
// { seq, t, first?, format: 'ticker-v1', text, mmsi, lat?, lon?, level, count? }
// text is already worded by the producer, including names and repeat counts.
// Coordinates locate the event; mmsi identifies its primary click target.
// Producers escape each literal backslash, *, :, [ and ] with a backslash
// before wrapping fields in markers. JSON encoding is a separate final step.
// ticker-v1: **bold**, ::muted:: and [[accent]]. Backslash escapes the next
// character. No nesting or HTML; unknown/unclosed markers remain literal text.
const styles = { '**': ['**', 'tk-name'], '::': ['::', 'tk-label'], '[[': [']]', 'tk-to'] };
const escapeHtml = (s) => s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

export function renderEventText(input) {
    const source = String(input ?? '');
    let text = '', html = '', plain = '';
    const append = (value, style) => {
        text += value;
        html += style ? `<span class="${style}">${escapeHtml(value)}</span>` : escapeHtml(value);
    };
    for (let i = 0; i < source.length;) {
        if (source[i] === '\\' && i + 1 < source.length) { plain += source[i + 1]; i += 2; continue; }
        const marker = source.slice(i, i + 2), spec = styles[marker];
        if (spec) {
            let j = i + 2, value = '', closed = false;
            for (; j < source.length;) {
                if (source[j] === '\\' && j + 1 < source.length) { value += source[j + 1]; j += 2; continue; }
                if (source.slice(j, j + 2) === spec[0]) { closed = true; break; }
                value += source[j++];
            }
            if (closed) {
                append(plain); plain = '';
                append(value, spec[1]); i = j + 2; continue;
            }
        }
        plain += source[i++];
    }
    append(plain);
    return { text, html };
}
