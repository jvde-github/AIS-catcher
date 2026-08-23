/* ============================================================================
   panel.js — the settings window: groups of tabs over sections of fields

   The host writes its settings as plain markup inside `.settings-main`:
   a <header> naming a page, then <section>s of fields, repeated. This turns
   that into a tab bar of groups, a row of sub-tabs where a group has more
   than one page, shows one page at a time, and opens and closes the window.
   Which controls exist and what they set is the host's; it listens on them.

       const panel = AISPanel.create({
           window: document.getElementById("settings"),
           groups: [{ title: "Map", subs: [["Map", "Map"], ["Tracks", "Tracks"]] }],
           onOpen: () => syncControlsFromSettings(),
       });
       panel.build();
       panel.open("Map");

   `groups` maps page names (the <header> text) to tabs: [name, label]. Pages
   not listed get a tab of their own, after the listed ones.

   Controls that simply hold a setting carry data-setting="key"; bind() keeps
   them and a store in step (checkbox -> boolean, range/number -> number):

       panel.bind(store, { onChange: (key, value) => redraw() });   // store: settings.js, or { get, set }
       panel.sync();                                                 // controls <- store, e.g. on open
   ========================================================================= */

import { tabScroller } from "./components.js";

var TAGS = [
    [".settings-header", "band"],
    [".settings-header .hdr-slot", "band-slot"],
    [".settings-main > header", "col-header"],
    [".settings-main section", "fieldset"],
    [".settings-main section div", "field"],
    [".settings-main section div > span:first-child", "field-label"],
    [".settings-main .st-desc", "field-desc"],
    [".settings-main select", "input select"],
    [".settings-main .color-picker", "input"],
    [".settings-main input[type=checkbox]", "toggle"],
    [".settings-main input[type=range]", "slider"],
    [".settings-main button:not(.tab-scroll-btn), .settings-main section div > span[data-action]", "btn"],
];

/* The panel is written as sections/divs/spans; the component classes are put
   on here so components.css needs no knowledge of the markup. */
export function adoptClasses(root) {
    TAGS.forEach(function (t) {
        root.querySelectorAll(t[0]).forEach(function (el) {
            t[1].split(" ").forEach(function (c) { el.classList.add(c); });
        });
    });
}

export function create(opts) {
    var win = opts.window;
    var main = win.querySelector(".settings-main");
    var headerSlot = win.querySelector(".settings-header .hdr-slot");
    var groupSpec = opts.groups || [];
    var onOpen = opts.onOpen || function () {};
    var onClose = opts.onClose || function () {};
    var skipGrouping = opts.skipGrouping || ".st-group, .filter-checks";

    var groups = [];
    var subNav = null, subWrap = null;
    var scrollers = [];

    function makeTab(label, activate) {
        var tab = document.createElement("div");
        tab.className = "settings-tab";
        tab.textContent = label;
        tab.setAttribute("role", "tab");
        tab.tabIndex = 0;
        tab.onclick = activate;
        tab.onkeydown = function (e) {
            if (e.key === "Enter" || e.key === " ") { e.preventDefault(); activate(); }
        };
        return tab;
    }

    /* the rows of a section sit in one box so they share a surface */
    function groupRows() {
        main.querySelectorAll("section").forEach(function (section) {
            if (section.querySelector(skipGrouping)) return;
            var rows = Array.prototype.filter.call(section.children, function (el) { return el.tagName !== "H5"; });
            if (!rows.length) return;
            var box = document.createElement("div");
            box.className = "st-group";
            rows.forEach(function (row) { box.appendChild(row); });
            section.appendChild(box);
        });
    }

    function scroller(nav, extraClass) {
        var r = tabScroller(nav, extraClass);
        scrollers.push(r.update);
        return r.wrap;
    }

    function build() {
        adoptClasses(win);
        groupRows();

        var parsed = {};
        var current = null;
        Array.prototype.forEach.call(main.children, function (child) {
            if (child.tagName === "HEADER") {
                current = child.textContent.trim();
                parsed[current] = { header: child, sections: [] };
            } else if (child.tagName === "SECTION" && current) {
                parsed[current].sections.push(child);
            }
        });
        if (Object.keys(parsed).length < 2) return;

        groups = groupSpec
            .map(function (g) {
                return {
                    title: g.title,
                    subs: g.subs.filter(function (s) { return s[0] in parsed; })
                        .map(function (s) { return Object.assign({ label: s[1] }, parsed[s[0]]); }),
                };
            })
            .filter(function (g) { return g.subs.length > 0; });

        var mapped = {};
        groupSpec.forEach(function (g) { g.subs.forEach(function (s) { mapped[s[0]] = true; }); });
        Object.keys(parsed).forEach(function (name) {
            if (!mapped[name]) groups.push({ title: name, subs: [Object.assign({ label: name }, parsed[name])] });
        });

        var nav = document.createElement("nav");
        nav.className = "settings-tabs";
        nav.setAttribute("role", "tablist");
        groups.forEach(function (g, i) { nav.appendChild(makeTab(g.title, function () { selectGroup(i); })); });
        if (headerSlot) headerSlot.appendChild(scroller(nav));

        subNav = document.createElement("nav");
        subNav.className = "settings-tabs settings-subtabs";
        subNav.setAttribute("role", "tablist");
        subWrap = scroller(subNav, "settings-nav");
        main.prepend(subWrap);

        selectGroup(0);
    }

    function syncScrollers() {
        scrollers.forEach(function (s) { s(); });
    }

    function selectGroup(idx) {
        var group = groups[idx];
        if (!group) return;
        win.querySelectorAll(".settings-tabs:not(.settings-subtabs) .settings-tab").forEach(function (t, i) {
            t.classList.toggle("active", i === idx);
            t.setAttribute("aria-selected", i === idx);
        });

        subNav.innerHTML = "";
        subWrap.style.display = group.subs.length > 1 ? "" : "none";
        group.subs.forEach(function (sub, i) {
            subNav.appendChild(makeTab(sub.label, function () { selectSub(idx, i); }));
        });
        selectSub(idx, 0);
        subNav.scrollLeft = 0;
        syncScrollers();
    }

    function selectSub(groupIdx, subIdx) {
        var shown = groups[groupIdx].subs[subIdx].sections;
        var first = true;
        Array.prototype.forEach.call(main.children, function (child) {
            if (child.tagName === "HEADER") child.style.display = "none";
            else if (child.tagName === "SECTION") {
                var show = shown.indexOf(child) >= 0;
                child.style.display = show ? "" : "none";
                child.classList.toggle("st-first", show && first);
                if (show) first = false;
            }
        });
        subNav.querySelectorAll(".settings-tab").forEach(function (t, i) {
            t.classList.toggle("active", i === subIdx);
            t.setAttribute("aria-selected", i === subIdx);
        });
        main.scrollTop = 0;
    }

    /* --- data-setting controls ------------------------------------------------ */

    var store = null;
    var onChange = null;

    function controls() {
        return win.querySelectorAll("[data-setting]");
    }

    function readControl(el) {
        if (el.type === "checkbox") return el.checked;
        if (el.type === "range" || el.type === "number") return Number(el.value);
        return el.value;
    }

    function sync() {
        if (!store) return;
        controls().forEach(function (el) {
            var v = store.get(el.dataset.setting);
            if (el.type === "checkbox") el.checked = !!v;
            else if (v != null) el.value = v;
        });
    }

    function bind(s, o) {
        store = s;
        onChange = (o && o.onChange) || null;
        win.addEventListener("change", function (e) {
            var el = e.target.closest("[data-setting]");
            if (!el) return;
            var key = el.dataset.setting, value = readControl(el);
            store.set(key, value);
            if (onChange) onChange(key, value, el);
        });
        sync();
    }

    function isOpen() { return win.classList.contains("active"); }

    function open(title) {
        sync();
        onOpen();
        win.classList.add("active");
        if (title != null) {
            var idx = -1;
            groups.forEach(function (g, i) { if (g.title === title) idx = i; });
            if (idx >= 0) selectGroup(idx);
        }
    }

    function close() {
        if (!isOpen()) return;
        win.classList.remove("active");
        onClose();
    }

    return {
        build: build,
        open: open,
        close: close,
        isOpen: isOpen,
        bind: bind,
        sync: sync,
        selectGroup: selectGroup,
        syncScrollers: syncScrollers,
        groups: function () { return groups; },
    };
}
