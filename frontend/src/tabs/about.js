import { marked } from 'marked';

let loaded = false;

export async function setup() {
    if (loaded) return;

    let text;
    try {
        const response = await fetch("about.md");
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        text = await response.text();
    } catch (error) {
        window.AISCatcher.showNotification("Error loading about.md: " + error);
        return;
    }

    document.getElementById("about_content").innerHTML = marked.parse(text);
    loaded = true;
}
