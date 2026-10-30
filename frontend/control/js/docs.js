// Links from a setting to where the documentation explains it
import { icon } from './icons.js';

const BASE = 'https://docs.aiscatcher.org/';

export const DOCS = {
    stations: 'usage/json-configuration/#stations-stations-array',
    gps: 'advanced/GPS/',
    services: 'usage/json-configuration/#tracking-and-services-tracking-services',
    api: 'api/overview/#the-read-api',
    keys: 'api/overview/#keys',
    mcp: 'api/overview/#mcp-an-assistant-reads-the-station',
    system: 'api/overview/#the-system-service-and-health',
    viewer: 'configuration/output/web-viewer/',
    prometheus: 'configuration/output/web-viewer/#sending-data-to-prometheus-for-use-in-grafana-dashboards',
    places: 'configuration/output/places/',
    homeAssistant: 'api/home-assistant/',
    webhooks: 'api/webhooks/'
};

export const docsUrl = path => BASE + path;

// a "Docs" link that opens beside the panel
export function docLink(path, label = 'Docs') {
    const a = document.createElement('a');
    a.className = 'st-doc';
    a.href = docsUrl(path);
    a.target = '_blank';
    a.rel = 'noopener';
    a.title = 'Open the documentation';
    a.innerHTML = icon('docs');
    a.append(label);
    return a;
}
