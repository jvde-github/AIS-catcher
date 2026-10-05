// The about tab: the station's about.md, rendered once per revision of it.

import { marked } from 'marked';
import { config } from '../../core/config.js';
import { showNotification } from '../../dialog.js';

let loadedVersion = null;

export async function setup() {
    const version = config.about_version || "initial";
    if (loadedVersion === version) return;

    let text;
    try {
        const response = await fetch("about.md", {cache: "no-store"});
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        text = await response.text();
    } catch (error) {
        showNotification("Error loading about.md: " + error, "error");
        return;
    }

    document.getElementById("about_content").innerHTML = marked.parse(text);
    loadedVersion = version;
}
