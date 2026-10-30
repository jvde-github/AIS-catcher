// Find ports and Find devices: what the station sees attached, to pick one from.
import * as AISComponents from '@aiscatcher/ui/components.js';
import { el } from './dom.js';
import { api } from './api.js';

function picker({ id, title, endpoint, extract, renderRow }) {
    return onPick => {
        const modal = AISComponents.modal({ id, title, footerLabel: 'Close' });
        const list = modal.body;
        list.innerHTML = '<div class="t-center t-muted sys-empty">Loading...</div>';
        modal.open();
        api(endpoint)
            .then(data => {
                list.innerHTML = '';
                const devices = extract(data);
                if (devices.length === 0) {
                    list.innerHTML = '<li class="t-center t-muted sys-empty">No devices found</li>';
                    return;
                }
                devices.forEach(device => {
                    list.appendChild(renderRow(device, () => {
                        onPick(device);
                        modal.close();
                    }));
                });
            })
            .catch(() => { list.innerHTML = '<li class="t-center t-danger sys-empty">Error loading devices</li>'; });
    };
}

export const pickSerialPort = picker({
    id: 'cm-serial-modal',
    title: 'Select Serial Device',
    endpoint: '/api/serial',
    extract: data => Array.isArray(data) ? data : [],
    renderRow: (device, select) => el('li', 'sys-pick', { onClick: select }, device)
});

export const pickDevice = picker({
    id: 'cm-device-modal',
    title: 'Select Device',
    endpoint: '/api/devices',
    extract: data => Array.isArray(data.devices) ? data.devices : [],
    renderRow: (device, select) => el('li', 'row row-between box sys-pick', {},
        el('span', '', {}, device.name),
        el('button', 'btn', { type: 'button', onClick: select }, 'Select'))
});
