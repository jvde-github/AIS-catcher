import { marked } from 'marked';

let loaded = false;

export async function setup() {
    if (loaded) return;

    let response;
    try {
        response = await fetch("about.md");
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
    } catch (error) {
        alert("Error loading about.md: " + error);
        for (const id of ['about_tab', 'about_tab_mini']) {
            const el = document.getElementById(id);
            if (el) el.style.display = 'none';
        }
        return;
    }

    const text = await response.text();
    document.getElementById("about_content").innerHTML = marked.parse(text);
    loaded = true;
}
