import { registerActions } from '../events.js';

export function decodeNMEA() {
    const input = document.getElementById('decoder_input').value.trim();
    const statusEl = document.getElementById('decoder_status');
    const outputEl = document.getElementById('decoder_output');

    if (!input) {
        statusEl.innerHTML = '<span class="decoder-error">Please enter NMEA sentences to decode.</span>';
        return;
    }

    statusEl.innerHTML = '<span class="decoder-info">Decoding...</span>';
    outputEl.innerHTML = '';

    fetch('api/decode', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: input
    })
        .then(response => response.json())
        .then(data => {
            if (data.error) {
                statusEl.innerHTML = `<span class="decoder-error">Error: ${data.error}</span>`;
                return;
            }

            if (Array.isArray(data) && data.length === 0) {
                statusEl.innerHTML = '<span class="decoder-warning">No valid AIS messages found in input.</span>';
                return;
            }

            statusEl.innerHTML = `<span class="decoder-success">Successfully decoded ${data.length} message(s)</span>`;

            const excludeFields = ['class', 'device', 'driver', 'hardware', 'version', 'scaled', 'signalpower', 'ppm', 'rxtime'];
            let html = '';
            data.forEach((msg, index) => {
                html += `<div class="decoder-message">`;
                html += `<h4>Message ${index + 1}</h4>`;
                html += '<table class="decoder-table">';

                Object.keys(msg).forEach(key => {
                    if (excludeFields.includes(key)) return;
                    const field = msg[key];
                    if (typeof field === 'object' && field !== null) {
                        const value = field.value !== undefined ? field.value : '';
                        const unit = field.unit || '';
                        const description = field.description || '';
                        const text = field.text || '';

                        let keyCol = key;
                        if (unit) keyCol += ` (${unit})`;

                        let valueCol = value;
                        if (text) valueCol += ` (${text})`;

                        html += '<tr>';
                        html += `<td class="decoder-field-key">${keyCol}</td>`;
                        html += `<td class="decoder-field-value">${valueCol}</td>`;
                        html += `<td class="decoder-field-description">${description}</td>`;
                        html += '</tr>';
                    }
                });

                html += '</table></div>';
            });

            outputEl.innerHTML = html;
        })
        .catch(error => {
            statusEl.innerHTML = `<span class="decoder-error">Network error: ${error.message}</span>`;
            console.error('Decode error:', error);
        });
}

export function clearDecoder() {
    document.getElementById('decoder_input').value = '';
    document.getElementById('decoder_output').innerHTML = '';
    document.getElementById('decoder_status').innerHTML = '';
}

export function init() {
    if (typeof decoder_enabled === 'undefined' || !decoder_enabled) {
        document.getElementById('decoder_tab').style.display = 'none';
        document.getElementById('decoder_tab_mini').style.display = 'none';
        return;
    }
    registerActions({ decodeNMEA, clearDecoder });
}
