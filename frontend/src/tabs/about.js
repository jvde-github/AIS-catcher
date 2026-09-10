import { marked } from 'marked';

let loadedVersion = null;

export async function setup() {
    const version = window.AISCatcher.config.about_version || "initial";
    if (loadedVersion === version) return;

    let text;
    try {
        const response = await fetch("about.md", {cache: "no-store"});
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        text = await response.text();
    } catch (error) {
        window.AISCatcher.showNotification("Error loading about.md: " + error, "error");
        return;
    }

    document.getElementById("about_content").innerHTML = marked.parse(text);
    loadedVersion = version;
}
