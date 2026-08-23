/* ============================================================================
   menu.js — a context menu (layer 2)

   A list of actions that opens at a point, beside an anchor, or centred, and
   closes on the next click outside. Items carry data-action (the host
   dispatches on it), data-group (a hairline separates runs of different
   groups), and any tags the host uses to decide which items a context shows.

       const menu = AISMenu.create({
           mount: document.getElementById("context-menu"),
           checks: { toggleLabels: () => settings.show_labels },     // action -> checked?
       });
       menu.add([
           { action: "copyMmsi", icon: "content_copy", label: "Copy MMSI", group: "text", tags: ["ship"] },
       ]);
       menu.open({ x: e.pageX, y: e.pageY, tags: ["ship", "ship-map"], onClose });

   open({ tags }) shows an item when one of its tags is in the list; an item
   with no tags is always shown. `show(li)` instead decides per item (true,
   false, or undefined to leave the tag rule's answer).
   ========================================================================= */

import { placeMenu, dismissOnce } from "./components.js";

export function create(opts) {
    var mount = opts.mount;
    var checks = opts.checks || {};
    var closer = null;

    function tagsOf(li) {
        return li.dataset.tags ? li.dataset.tags.split(/\s+/) : [];
    }

    function add(items) {
        var frag = document.createDocumentFragment();
        items.forEach(function (it) {
            var li = document.createElement("li");
            if (it.action) li.dataset.action = it.action;
            if (it.group) li.dataset.group = it.group;
            if (it.tags && it.tags.length) li.dataset.tags = it.tags.join(" ");
            if (it.id) li.id = it.id;
            Object.keys(it.data || {}).forEach(function (k) { li.dataset[k] = it.data[k]; });

            if (it.icon) {
                var i = document.createElement("i");
                i.className = it.icon + "_icon";
                li.appendChild(i);
            }
            var span = document.createElement("span");
            span.textContent = it.label || "";
            if (it.labelId) span.id = it.labelId;
            li.appendChild(span);
            if (it.check) {
                var c = document.createElement("span");
                c.className = "ctx-check";
                c.textContent = "\u2713";
                li.appendChild(c);
            }
            frag.appendChild(li);
        });
        if (opts.before) mount.insertBefore(frag, opts.before);
        else mount.appendChild(frag);
    }

    function setLabel(action, text) {
        var li = mount.querySelector('li[data-action="' + action + '"]');
        if (!li) return;
        var span = li.querySelector("span");
        if (span) span.textContent = text; else li.textContent = text;
    }

    function applyChecks() {
        mount.querySelectorAll("li[data-action]").forEach(function (li) {
            var fn = checks[li.dataset.action];
            if (fn) li.classList.toggle("checked", !!fn(li));
        });
    }

    /* a hairline between runs of different groups, only where a run has
       more than one item: a lone item between two lone items is not a group */
    function applySeparators() {
        var runs = [];
        mount.querySelectorAll("li").forEach(function (li) {
            li.classList.remove("group-start");
            if (li.style.display === "none") return;
            var last = runs[runs.length - 1];
            if (last && last.group === li.dataset.group) last.items.push(li);
            else runs.push({ group: li.dataset.group, items: [li] });
        });
        for (var i = 1; i < runs.length; i++) {
            if (runs[i - 1].items.length > 1 || runs[i].items.length > 1) runs[i].items[0].classList.add("group-start");
        }
    }

    function open(o) {
        o = o || {};
        close();
        var tags = o.tags || [];
        mount.querySelectorAll("li").forEach(function (li) {
            var own = tagsOf(li);
            var byTag = own.length === 0 || own.some(function (t) { return tags.indexOf(t) >= 0; });
            var v = o.show ? o.show(li, byTag) : undefined;
            li.style.display = (v === undefined ? byTag : v) ? "flex" : "none";
        });

        applyChecks();
        applySeparators();

        placeMenu(mount, { anchor: o.anchor, center: !o.anchor && !!o.center, x: o.x || 0, y: o.y || 0 });

        closer = dismissOnce(mount, function () {
            closer = null;
            if (o.onClose) o.onClose();
        });
    }

    function close() {
        if (closer) closer();
    }

    return {
        el: mount,
        add: add,
        setLabel: setLabel,
        open: open,
        close: close,
        isOpen: function () { return !!closer; },
    };
}
