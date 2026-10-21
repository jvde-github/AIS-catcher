// Links from a setting to where the documentation explains it

const BASE = 'https://docs.aiscatcher.org/';
const ICON = '<svg class="st-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 5.5A2.5 2.5 0 0 1 6.5 3H20v15H6.5A2.5 2.5 0 0 0 4 20.5z"/><path d="M4 20.5A2.5 2.5 0 0 0 6.5 23H20v-5"/></svg>';

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
    places: 'configuration/output/places/'
};

// an input's page, by its device
const INPUTS = {
    RTLSDR: 'configuration/input/rtlsdr/', AIRSPY: 'configuration/input/airspy/', AIRSPYHF: 'configuration/input/airspyhf/',
    HACKRF: 'configuration/input/hackrf/', HYDRASDR: 'configuration/input/hydrasdr/', SDRPLAY: 'configuration/input/sdrplay/',
    SOAPYSDR: 'configuration/input/soapysdr/', SERIALPORT: 'configuration/input/serial/', UDPSERVER: 'configuration/input/udp/',
    RTLTCP: 'configuration/input/tcp/', SPYSERVER: 'configuration/input/spyserver/', NMEA2000: 'configuration/input/NMEA2000/',
    WAVFILE: 'configuration/input/wav/', FILE: 'configuration/input/file/'
};
export const inputDocs = input => INPUTS[String(input || '').toUpperCase()] || 'configuration/input/overview/';

// an output type's page, by the Output tab's name for it
const OUTPUTS = {
    sharing: 'configuration/output/community-feed/', udp: 'configuration/output/UDP/', http: 'configuration/output/HTTP/',
    mqtt: 'configuration/output/MQTT/', tcp: 'configuration/output/TCP-client/', 'tcp-server': 'configuration/output/TCP-server/',
    db: 'configuration/output/PSQL/', server: 'configuration/output/web-viewer/', webhooks: 'api/webhooks/',
    mqtt_state: 'api/home-assistant/', screen: 'configuration/output/console/'
};
export const outputDocs = type => OUTPUTS[type] || 'configuration/output/overview/';

export const docsUrl = path => BASE + path;

// a "Docs" link that opens beside the panel
export function docLink(path, label = 'Docs') {
    const a = document.createElement('a');
    a.className = 'st-doc';
    a.href = docsUrl(path);
    a.target = '_blank';
    a.rel = 'noopener';
    a.title = 'Open the documentation';
    a.innerHTML = ICON;
    a.append(label);
    return a;
}
