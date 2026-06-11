// Generic helpers with no app state.

export function debounce(func, wait) {
    let timeout;
    function debounced(...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(this, args), wait);
    }
    debounced.cancel = function () {
        clearTimeout(timeout);
    };
    return debounced;
}

export function decodeHTMLEntities(text) {
    const textArea = document.createElement('textarea');
    textArea.innerHTML = text;
    return textArea.value;
}

export function hexToRgb(hex) {
    const m = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex || '');
    return m ? [parseInt(m[1], 16), parseInt(m[2], 16), parseInt(m[3], 16)] : [18, 165, 237];
}

// Derive a readable label background from a (often bright) track color:
// boost saturation by pushing channels away from luma, then darken.
export function deriveLabelBackground(hex) {
    let [r, g, b] = hexToRgb(hex);
    const luma = 0.299 * r + 0.587 * g + 0.114 * b;
    const saturate = 1.35, darken = 0.6;
    const adjust = (c) => Math.max(0, Math.min(255, Math.round((luma + (c - luma) * saturate) * darken)));
    return `rgba(${adjust(r)}, ${adjust(g)}, ${adjust(b)}, 0.88)`;
}

export async function copyToClipboard(textToCopy) {
    // Navigator clipboard api needs a secure context (https)
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(textToCopy);
    } else {
        // Use the 'out of viewport hidden text area' trick
        const textArea = document.createElement("textarea");
        textArea.value = textToCopy;

        // Move textarea out of the viewport so it's not visible
        textArea.style.position = "absolute";
        textArea.style.left = "-999999px";

        document.body.prepend(textArea);
        textArea.select();

        try {
            document.execCommand("copy");
        } catch (error) {
            console.error(error);
        } finally {
            textArea.remove();
        }
    }
}
