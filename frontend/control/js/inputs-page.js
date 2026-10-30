// The Inputs page: one input to a page, the device, how to reach it, its station, its settings.
import { icon } from './icons.js';
import { DOCS, docLink } from './docs.js';
import { decodes, describeInput, inputDocs, inputGroup, INPUT_ESSENTIALS } from './inputs.js';
import { pickDevice, pickSerialPort } from './device-picker.js';
import { ConfigForm } from './config-form.js';
import { createStationPicker } from './station-picker.js';
import { inputRouting } from './flow-editor.js';
import { receiverSchema } from './schema.js';
import { el, isActive } from './dom.js';

// the Find button beside the control it fills: Find ports on the port line, Find devices
// on the type line, as it sets the type and the serial together
function addFindButton(form, node, item) {
    const serialPort = item.input === 'SERIALPORT';
    const target = node.querySelector(`[data-field="${serialPort ? 'serialport_port' : 'input'}"]`);
    const ctrl = target && [...target.children].find(c => c.matches('input, select') || c.querySelector('input, select'));
    if (!ctrl) return;
    const picked = apply => choice => { apply(choice); form.markDirty(); form.render(); };
    const find = serialPort
        ? () => pickSerialPort(picked(port => { (item.serialport ||= {}).port = port; }))
        : () => pickDevice(picked(device => { item.input = device.input ? device.input.toUpperCase() : ''; item.serial = device.serial || ''; }));
    const btn = el('button', 'st-btn', { type: 'button', onClick: find, html: icon('search') }, serialPort ? 'Find ports' : 'Find devices');
    const row = el('div', 'st-inline');
    ctrl.replaceWith(row);
    row.append(ctrl, btn);
}

// the station the antenna is, picked from the list, under the device
function stationSection(form, item, index) {
    const stationField = form.config.stationPicker && form.fields.find(f => f.name === 'station');
    if (!stationField) return null;
    const picker = form.config.stationPicker(item.station, id => form.updateValue(index, stationField, id));
    const desc = stationField.description || stationField.tooltip;
    return el('div', 'st-flat st-stpick__section', {},
        el('div', 'st-flat__head', {}, el('h3', 'st-section__title', {}, 'Station'), form.config.stationDocs?.()),
        picker, desc ? el('p', 'st-help', {}, desc) : null);
}

// the Inputs page: the selected input, the rail's list says which
export function inputsLayout(form) {
    form.container.className = 'st st-single';
    form.selected = Math.min(form.selected ?? 0, Math.max(0, form.data.length - 1));
    const main = el('div', 'st-main');
    if (!form.data.length) {
        main.appendChild(el('div', 'st-empty', {}, el('strong', '', {}, 'No inputs yet'),
            el('span', '', {}, 'Add an input to tell AIS-catcher where to receive from.')));
    } else {
        const index = form.selected, item = form.data[index];
        // the title follows the name, the device's own name until one is given; the routing
        // line says where this input's messages go
        const fallback = () => describeInput({ ...item, description: '' }, index, form.data).title;
        const { head, name, title, onZones } = form.focusHead(item, index,
            { title: describeInput(item, index, form.data).title, fallback, dir: 'out' });
        main.append(head, ...(name ? [name] : []));
        const station = stationSection(form, item, index);
        const { node, refresh } = form.renderSplit(item, index, { essentials: form.config.essentials || new Set(), key: 'input', onZones,
            // settings that cannot apply to this kind of input stay out (engines decode radio only)
            exclude: new Set(['description', ...(station ? ['station'] : []), ...(form.config.excludeFor ? form.config.excludeFor(item) : [])]),
            flat: {}, lead: station ? [station] : [] });
        addFindButton(form, node, item);
        node.dataset.receiverCard = index;
        main.appendChild(node);
        // the title and the name's placeholder follow the device and serial as they change
        node._sync = () => { if (!(item.description || '').trim()) title.textContent = fallback(); if (name) name.querySelector('input').placeholder = fallback(); };
        refresh();
    }
    form.container.append(main);
    return main;
}

// The page in `host`, a form of the configuration's receivers. `lost(item)`: why the running
// receiver has lost an input; `stations()` the stations to pick from, `stationCreated` when one is
// made here; `open(tab, key)` a page of the rail; `onChange(form)` after an edit.
export function createInputsPage(host, { lost, stations, stationCreated, open, onChange }) {
    const containerId = 'hub-receivers-container';
    host.replaceChildren(el('div', '', { id: containerId }));
    const form = new ConfigForm({
        section: 'receiver',
        list: true,
        schema: receiverSchema,
        containerId,
        layout: inputsLayout,
        title: 'Receiver',
        what: 'input',
        titleOf: (item, i) => describeInput(item, i, form.data).title,
        essentials: INPUT_ESSENTIALS,
        kindOf: item => describeInput(item, 0, [item]).kind,
        routing: inputRouting,
        problem: item => {
            const why = isActive(item) && lost(item);
            return why ? { tone: 'bad', text: why } : null;
        },
        excludeFor: item => ['zone', ...(decodes(item.input) ? [] : ['engines'])],
        groupOf: inputGroup,
        docs: item => docLink(inputDocs(item.input)),
        stationDocs: () => docLink(DOCS.stations),
        stationPicker: (value, onPick, opts = {}) => createStationPicker(value, { stations, onPick, onCreated: stationCreated, ...opts }),
        openFlow: () => open('flow'),
        // an input switched on without a device type fails every save, whatever else it holds
        check: rows => {
            const i = rows.findIndex(r => isActive(r) && !String(r.input || '').trim() && !String(r.serial || '').trim());
            if (i < 0) return null;
            open('input', i);
            return `${describeInput(rows[i], i, rows).title}: choose its device type, or delete it.`;
        },
        onChange
    });
    return form;
}
