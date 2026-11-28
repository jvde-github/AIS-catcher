// The hub's configuration schema: what each section holds, how it is shown and checked.

import { decodes, DEVICES, RADIOS } from './inputs.js';

// a field's name is its key, unless it gives another; `stored`: kept in the file under that name
const fields = (specs, stored = false) => Object.fromEntries(Object.entries(specs).map(([key, f]) => {
    const name = f.name ?? key;
    return [key, stored ? { name, jsonpath: name, ...f } : { name, ...f }];
}));
const stored = specs => fields(specs, true);

// a device's settings, in its own block of the file and shown for that kind of input only
const device = (prefix, input, specs) => Object.fromEntries(Object.entries(specs).map(([key, f]) =>
    [`${prefix}_${key}`, { name: `${prefix}_${key}`, jsonpath: `${prefix}.${key}`, dependsOn: { field: 'input', value: input }, ...f }]));

// each a value that is its own label, or [value, label]
const opts = (...list) => list.map(o => Array.isArray(o) ? { value: o[0], label: o[1] } : { value: o, label: o });

export const ChannelFields = {
    host: () => ({ name: 'host', section: 'Connection', label: 'Address', type: 'text', defaultValue: '127.0.0.1', placeholder: 'e.g., 192.168.1.101', width: 75 }),
    port: () => ({ name: 'port', section: 'Connection', label: 'Port', type: 'number', defaultValue: 10110, placeholder: '10110', width: 25 }),
    description: () => ({ name: 'description', section: 'Label', label: 'Description', type: 'text', placeholder: 'Optional description', width: 50 }),
    link: () => ({ name: 'link', section: 'Label', label: 'Web Page', type: 'text', placeholder: 'https://…', width: 50,
        tooltip: 'Your page at this service, linked from Statistics' }),
    active: () => ({ name: 'active', section: 'Options', label: 'Active', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Pause without removing' }),
    unique: () => ({ name: 'unique', section: 'Filter', label: 'Unique', type: 'toggle', defaultValue: false, width: 50, tooltip: 'Skip repeats within 3 seconds' }),
    msgformat: (defaultValue = 'NMEA') => ({
        name: 'msgformat', section: 'Format', label: 'Message Format', type: 'select', defaultValue, tooltip: 'Format of forwarded messages',
        options: opts('NMEA', ['NMEA_TAG', 'NMEA + TAG'], ['JSON_NMEA', 'JSON with NMEA'], ['JSON_SPARSE', 'JSON (compact)'], ['JSON_FULL', 'JSON (full)'],
            ['JSON_ANNOTATED', 'JSON (annotated)'], ['BINARY_NMEA', 'Binary'])
    }),
    position_interval: () => ({
        name: 'position_interval', section: 'Filter', label: 'Downsample Position', type: 'switch-integer', tooltip: 'One position per vessel per interval',
        width: 50, defaultValue: false, defaultInteger: 60, min: 0, max: 3600, step: 1
    })
};

export const sdrGainFields = (prefix, inputValue) => {
    const mode = value => ({ field: `${prefix}_gain_mode`, value });
    return device(prefix, inputValue, {
        gain_mode: { label: 'Gain Mode', type: 'select', defaultValue: 'linearity', options: opts(['free', 'Free'], ['linearity', 'Linearity'], ['sensitivity', 'Sensitivity']),
            tooltip: 'Free sets each stage' },
        vga: { label: 'VGA', type: 'number', min: 0, max: 14, step: 1, defaultValue: 10, placeholder: '0-14', width: 33, dependsOn: mode('free') },
        mixer: { label: 'Mixer', type: 'auto-integer', min: 0, max: 14, step: 1, defaultValue: 'auto', placeholder: 'auto or 0-14', width: 33, dependsOn: mode('free') },
        lna: { label: 'LNA', type: 'auto-integer', min: 0, max: 14, step: 1, defaultValue: 'auto', placeholder: 'auto or 0-14', width: 32, dependsOn: mode('free') },
        linearity: { label: 'Linearity', type: 'number', min: 0, max: 21, step: 1, defaultValue: 17, placeholder: '0-21', dependsOn: mode('linearity') },
        sensitivity: { label: 'Sensitivity', type: 'number', min: 0, max: 21, step: 1, defaultValue: 17, placeholder: '0-21', dependsOn: mode('sensitivity') },
        biastee: { label: 'Bias tee', type: 'toggle', tooltip: 'Power an external LNA', defaultValue: false, width: 25 }
    });
};

export const httpSchema = fields({
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    url: { section: 'Connection', label: 'URL', type: 'text', width: 75, placeholder: 'https://example.com', tooltip: 'Address to post to' },
    interval: { section: 'Connection', label: 'Interval (seconds)', type: 'number', min: 1, max: 86400, defaultValue: 60, placeholder: '60', width: 25,
        tooltip: 'How often messages are posted' },
    active: ChannelFields.active(),
    id: { section: 'Connection', label: 'ID', type: 'text', width: 25, placeholder: 'Station ID', tooltip: 'Station identifier in the feed' },
    userpwd: { section: 'Connection', label: 'Credentials', type: 'text', width: 75, placeholder: 'user:password', tooltip: 'Sent as user:password' },
    gzip: { section: 'Options', label: 'Gzip', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Compress posted data' },
    response: { section: 'Options', label: 'Response', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Log the server response' },
    protocol: { section: 'Format', label: 'Protocol', type: 'select', defaultValue: 'AISCATCHER', options: opts('AISCATCHER', 'MINIMAL', 'APRS', 'LIST', 'AIRFRAMES', 'NMEA'),
        tooltip: 'Format the server expects' },
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
});

export const udpSchema = fields({
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    active: ChannelFields.active(),
    broadcast: { section: 'Options', label: 'Broadcast', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Allow broadcast addresses' },
    persist: { section: 'Options', label: 'Retry', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Keep trying when the host is not found' },
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
});

export const tcpSchema = fields({
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    active: ChannelFields.active(),
    persist: { section: 'Options', label: 'Reconnect', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Retry after failures' },
    keep_alive: { section: 'Options', label: 'Keep Alive', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Enable TCP keep-alive probes' },
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
});

export const tcpServerSchema = fields({
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    port: { section: 'Connection', label: 'Port', type: 'number', defaultValue: 5010, placeholder: '5010', tooltip: 'Port clients connect to, max 64' },
    active: ChannelFields.active(),
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
});

// One channel for every backend: type picks the class, the rest is shared.
export const dbSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    ...stored({
        type: { section: 'Connection', label: 'Database', type: 'select', defaultValue: 'sqlite', width: 40, tooltip: 'Where messages are stored',
            options: opts(['sqlite', 'SQLite (local file)'], ['postgres', 'PostgreSQL'], ['csv', 'CSV files']) },
        conn_str: { section: 'Connection', label: 'Connection', type: 'text', placeholder: 'ais.db  ·  dbname=ais  ·  /var/log/ais', tooltip: 'File, connection string or folder',
            width: 60 },
        retention: { section: 'Retention', label: 'Retention (days)', type: 'number', min: 0, defaultValue: 0, tooltip: 'Days kept, 0 for all' },
        capacity: { label: 'State Capacity', type: 'number', min: 64, max: 1000000, defaultValue: 8192, tooltip: 'CSV: targets kept in the state file',
            dependsOn: { field: 'type', value: 'csv' } },
        station_id: { section: 'Station', label: 'Station ID', type: 'number', min: 0, tooltip: 'Overrides the station on every row' }
    }),
    active: ChannelFields.active(),
    ...stored({
        state: { section: 'Options', label: 'State', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Latest values per vessel' },
        stats: { section: 'Options', label: 'Statistics', type: 'toggle', defaultValue: true, width: 25, tooltip: 'Hourly reception statistics' },
        position: { section: 'Options', label: 'Positions', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Every position report' },
        dbstatic: { name: 'static', section: 'Options', label: 'Static', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Static and voyage reports' },
        nmea: { section: 'Options', label: 'Raw NMEA', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Keep raw sentences' },
        interval: { label: 'Write Interval (seconds)', type: 'number', min: 5, max: 1800, defaultValue: 60, tooltip: 'Seconds between writes' }
    }),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const mqttSchema = fields({
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    url: { section: 'Connection', label: 'URL', type: 'text', placeholder: 'mqtt[s]://[user:pass@]host[:port]', tooltip: 'Broker address' },
    active: ChannelFields.active(),
    topic: { section: 'Connection', label: 'Topic', type: 'text', placeholder: 'ais/data', defaultValue: 'ais/data', tooltip: 'Accepts %mmsi%, %type%, %channel%' },
    client_id: { section: 'Connection', label: 'Client ID', type: 'text', width: 50, placeholder: 'Optional client identifier', tooltip: 'Name the broker sees' },
    qos: { section: 'Connection', label: 'QoS', type: 'select', width: 50, defaultValue: '0', tooltip: 'Delivery guarantee',
        options: opts(['0', '0 - At most once'], ['1', '1 - At least once'], ['2', '2 - Exactly once']) },
    msgformat: ChannelFields.msgformat('JSON_FULL'),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
});

export const sharingSchema = fields({
    sharing: { label: 'Enable Sharing', type: 'toggle', defaultValue: false, tooltip: 'Share with the community map' },
    sharing_key: { label: 'Sharing Key (UUID)', type: 'text', placeholder: 'Enter UUID', tooltip: 'Leave empty to share anonymously' }
});

const HOSTED = ['txt', 'tls', 'none', 'rtltcp', 'wsmqtt', 'wssmqtt', 'mqtt', 'mqtts', 'gpsd', 'beast', 'basestation', 'raw1090'];
const WS = ['ws', 'wss'];
const MQTT = ['mqtt', 'mqtts', 'wsmqtt', 'wssmqtt'];
const over = value => ({ field: 'rtltcp_protocol', value: Array.isArray(value) ? [...value] : value });

// Viewer settings are applied through the versioned configuration feed.
export const receiverSchema = {
    description: { name: 'description', label: 'Description', type: 'text', placeholder: 'e.g. Mast antenna' },
    ...stored({
        station: { label: 'Station', type: 'number', min: 1, step: 1, placeholder: 'its own', tooltip: 'Where this antenna stands' },
        station_upstream: { label: 'From upstream', type: 'toggle', tooltip: 'a station the incoming data names comes first, where it is listed' },
        input: {
            label: 'Device Type', type: 'select',
            options: [{ value: '', label: 'Select a device' },
                ...Object.entries(DEVICES).filter(([, d]) => !d.hidden).map(([value, d]) => ({ value, label: d.label || d.name }))],
            // another device: the serial of the one before goes
            clears: ['serial']
        },
        channel: { label: 'Channel', type: 'select', defaultValue: 'AB', options: opts('AB', 'CD'), tooltip: 'AB standard, CD long range',
            dependsOn: { field: 'input', value: Object.keys(DEVICES).filter(decodes) } },
        nmea_channel: { label: 'Report As', type: 'select', options: opts(['', 'CD'], 'AB'), tooltip: 'Channel letters in NMEA output',
            dependsOn: { field: 'channel', value: 'CD' } },
        serial: { label: 'Serial Key', type: 'text', placeholder: 'Optional Serial Key', tooltip: 'Pick a device by serial number',
            dependsOn: { field: 'input', value: ['', ...RADIOS] } },
        active: { label: 'Active', type: 'toggle', defaultValue: true, width: 25 },
        engines: {
            label: '', type: 'engines', advanced: 'Engines', tooltip: 'A second engine doubles CPU',
            options: opts(['auto', 'Auto'], ['v1_base', 'AIS Engine v1 Base'], ['v1_high', 'AIS Engine v1 High'], ['v2_base', 'AIS Engine v2 Base'], ['v2_high', 'AIS Engine v2 High']),
            settings: [{ name: 'fp_ds', label: 'Fixed-point downsampling', type: 'toggle', types: ['v1_base', 'v1_high', 'v2_base', 'v2_high'], tooltip: 'Less CPU on small devices' }]
        }
    }),
    ...device('rtlsdr', 'RTLSDR', {
        tuner: { name: 'tuner', label: 'Tuner Gain', type: 'auto-float', min: 0, max: 50, step: 0.1, defaultValue: 'auto', tooltip: 'Gain in dB, or auto for AGC' },
        bandwidth: { label: 'Bandwidth (Hz)', type: 'integer-select', defaultValue: 0, min: 0, max: 1000000, placeholder: 'in Hz, e.g. 0 (Off), 192000 or 192K',
            presets: opts([0, 'Off'], [192000, '192K']), tooltip: 'Off uses the device default' },
        sample_rate: { label: 'Sample Rate (Hz)', type: 'integer-select', defaultValue: 1536000, min: 96000, max: 3200000, placeholder: 'in Hz, e.g. 1536000 or 1536K',
            presets: opts([288000, '288K'], [1536000, '1536K (default)']), tooltip: '1536K recommended; 288K saves CPU' },
        freqoffset: { label: 'Frequency Correction (ppm)', type: 'number', min: -150, max: 150, step: 0.1, defaultValue: 0, tooltip: 'Frequency correction; the dongle uses whole ppm' },
        biastee: { label: 'Bias tee', type: 'toggle', tooltip: 'Power an external LNA', defaultValue: false, width: 25 },
        rtlagc: { label: 'RTLAGC', type: 'toggle', tooltip: 'RTL2832U internal AGC', defaultValue: true, width: 25 }
    }),
    ...device('airspyhf', 'AIRSPYHF', {
        sample_rate: { label: 'Sample Rate (Hz)', type: 'integer-select', defaultValue: 192000, min: 192000, max: 768000, placeholder: 'in Hz, e.g. 192000 or 192K',
            presets: opts([192000, '192K'], [256000, '256K'], [384000, '384K'], [768000, '768K']), tooltip: 'Higher costs more CPU' },
        threshold: { label: 'Threshold', type: 'select', defaultValue: 'low', options: opts(['low', 'Low'], ['high', 'High']), tooltip: 'Automatic gain control threshold' },
        preamp: { label: 'Preamp', type: 'toggle', tooltip: 'Enable the built-in preamplifier', defaultValue: false, width: 25 }
    }),
    ...sdrGainFields('airspy', 'AIRSPY'),
    ...sdrGainFields('hydrasdr', 'HYDRASDR'),
    ...device('rtltcp', 'RTLTCP', {
        protocol: {
            label: 'Protocol', type: 'select', defaultValue: 'rtltcp', tooltip: 'How the server is reached',
            options: opts(['txt', 'NMEA'], ['tls', 'NMEA over TLS'], ['none', 'Raw'], ['rtltcp', 'RTLTCP'], ['ws', 'WS'], ['wss', 'WSS'], ['wsmqtt', 'WSMQTT'], ['wssmqtt', 'WSSMQTT'],
                ['mqtt', 'MQTT'], ['mqtts', 'MQTTS'], ['gpsd', 'GPSD'], ['beast', 'BEAST'], ['basestation', 'BASESTATION'], ['raw1090', 'RAW1090'])
        },
        host: { label: 'Host', type: 'text', placeholder: 'e.g., 127.0.0.1', tooltip: 'Server address', dependsOn: over(HOSTED), width: 75 },
        port: { label: 'Port', type: 'number', placeholder: 'e.g., 1234', tooltip: 'Server port', dependsOn: over(HOSTED), width: 25 },
        url: { label: 'URL', type: 'text', placeholder: 'wss://host:port/path?query', tooltip: 'user:password@ or token@ for login', dependsOn: over(WS) },
        ssl_verify: { label: 'Verify Certificate', type: 'toggle', defaultValue: true, width: 25, tooltip: "Check the server's certificate",
            dependsOn: over(['wss', 'tls', 'mqtts', 'wssmqtt']) },
        sample_rate: { label: 'Sample Rate (Hz)', type: 'number', min: 0, max: 20000000, defaultValue: 288000, placeholder: '0-20,000,000', tooltip: 'Must match the server',
            dependsOn: over('rtltcp'), width: 50 },
        bandwidth: { label: 'Bandwidth (Hz)', type: 'number', min: 0, max: 1000000, defaultValue: 0, placeholder: '0-1,000,000', tooltip: '0 uses the default',
            dependsOn: over('rtltcp'), width: 50 },
        freqoffset: { label: 'Frequency Offset (PPM)', type: 'number', min: -150, max: 150, step: 0.1, defaultValue: 0, placeholder: '-150 to +150', tooltip: 'Frequency correction; the dongle uses whole ppm',
            dependsOn: over('rtltcp') },
        tuner: { label: 'Tuner Gain', type: 'auto-float', min: 0, max: 50, step: 0.1, defaultValue: 'auto', tooltip: 'Gain in dB, or auto for AGC', dependsOn: over('rtltcp') },
        rtlagc: { label: 'RTL AGC', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Automatic gain on the dongle', dependsOn: over('rtltcp') },
        protocols: { label: 'Protocols', type: 'text', placeholder: 'WebSocket sub-protocols', tooltip: 'Subprotocols to offer the server', dependsOn: over(WS) },
        binary: { label: 'Binary Mode', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Server sends binary frames', dependsOn: over(WS) },
        origin: { label: 'Origin', type: 'text', placeholder: 'Origin header for WebSocket', tooltip: 'Origin header to send', dependsOn: over([...WS, 'wsmqtt', 'wssmqtt']) },
        topic: { label: 'Topic', type: 'text', defaultValue: 'ais/data', tooltip: 'Topic to subscribe to', dependsOn: over(MQTT) },
        client_id: { label: 'Client ID', type: 'text', placeholder: 'MQTT client identifier', tooltip: 'Name the broker sees', dependsOn: over(MQTT) },
        username: { label: 'Username', type: 'text', placeholder: 'Username (MQTT, or HTTP Basic for WebSocket)', tooltip: 'Login name', dependsOn: over([...MQTT, ...WS]) },
        password: { label: 'Password', type: 'text', placeholder: 'Password, or a bearer token with no username', tooltip: 'Login password', dependsOn: over([...MQTT, ...WS]) },
        qos: { label: 'QoS Level', type: 'select', options: opts('0', '1', '2'), defaultValue: '0', tooltip: 'Delivery guarantee', dependsOn: over(MQTT) }
    }),
    ...device('serialport', 'SERIALPORT', {
        port: { label: 'Port', type: 'text', placeholder: 'e.g., /dev/tty0' },
        baudrate: { label: 'Baud Rate', type: 'select', defaultValue: '38400', options: opts('9600', '19200', '38400', '57600', '115200'), tooltip: '38400 for most AIS receivers' },
        init_seq: { label: 'Init Sequence', type: 'text', tooltip: 'Sent when the port opens' },
        dump_file: { label: 'Dump File', type: 'text', placeholder: 'e.g., /var/log/ais-serial.log', tooltip: 'Write raw data to this file' }
    }),
    ...device('udpserver', 'UDPSERVER', {
        server: { label: 'Server Address', type: 'text', placeholder: 'e.g., 127.0.0.1', tooltip: 'Address to listen on', width: 75 },
        port: { label: 'Port', type: 'number', placeholder: 'e.g., 1234', tooltip: 'Port to listen on', width: 25 }
    }),
    ...device('tcpserver', 'TCPSERVER', {
        format: { label: 'Format', type: 'select', defaultValue: 'txt', tooltip: 'What the senders send',
            options: opts(['txt', 'NMEA'], ['n2k', 'NMEA 2000 frames'], ['basestation', 'BASESTATION'], ['raw1090', 'RAW1090']) },
        server: { label: 'Server Address', type: 'text', placeholder: 'e.g., 0.0.0.0', tooltip: 'IPv4 address to listen on', width: 75 },
        port: { label: 'Port', type: 'number', placeholder: 'e.g., 1234', tooltip: 'Port to listen on', width: 25 }
    }),
    ...device('hackrf', 'HACKRF', {
        lna: { label: 'LNA Gain', type: 'number', min: 0, max: 40, step: 8, defaultValue: 8, placeholder: '0-40 (in steps of 8)', tooltip: 'RF gain in dB', width: 50 },
        vga: { label: 'VGA Gain', type: 'number', min: 0, max: 62, step: 2, defaultValue: 20, placeholder: '0-62 (in steps of 2)', tooltip: 'Baseband gain in dB', width: 50 },
        preamp: { label: 'Preamp', type: 'toggle', tooltip: 'Enable the built-in preamplifier', defaultValue: false, width: 25 }
    }),
    ...device('sdrplay', 'SDRPLAY', {
        grdb: { label: 'Gain Reduction (dB)', type: 'number', min: 0, max: 59, step: 1, defaultValue: 32, placeholder: '0-59', tooltip: 'Less reduction, more gain', width: 50 },
        lnastate: { label: 'LNA State', type: 'number', min: 0, max: 9, step: 1, defaultValue: 0, placeholder: '0-9', tooltip: '0 is the most gain', width: 50 },
        agc: { label: 'AGC', type: 'toggle', defaultValue: false, tooltip: 'Automatic gain control', width: 25 },
        antenna: { label: 'Antenna', type: 'select', options: opts('A', 'B', 'C'), tooltip: 'The port on an RSP2 or RSPdx' },
        tuner: { label: 'Tuner', type: 'select', options: opts('A', 'B'), tooltip: 'The tuner on an RSPduo' }
    }),
    ...device('soapysdr', 'SOAPYSDR', {
        device: { label: 'Device', type: 'text', placeholder: 'e.g., driver=rtlsdr,serial=00000001', tooltip: "SoapySDR's device arguments" },
        gain: { label: 'Gain', type: 'text', placeholder: 'e.g., TUNER=40', tooltip: 'Per gain element, as name=value' },
        agc: { label: 'AGC', type: 'toggle', defaultValue: true, tooltip: 'Automatic gain control', width: 25 },
        antenna: { label: 'Antenna', type: 'text', tooltip: "The device's antenna by name", width: 50 },
        ch: { label: 'Channel', type: 'number', min: 0, max: 32, step: 1, defaultValue: 0, tooltip: "The device's receive channel", width: 50 },
        stream: { label: 'Stream Arguments', type: 'text', placeholder: 'name=value,…', tooltip: 'Passed to the stream as it opens' },
        setting: { label: 'Device Settings', type: 'text', placeholder: 'name=value,…', tooltip: 'Written to the device as it opens' }
    }),
    ...device('spyserver', 'SPYSERVER', {
        gain: { label: 'Tuner Gain', type: 'number', min: 0, max: 50, step: 0.1, defaultValue: 0, placeholder: 'Gain (0-50)', tooltip: 'Gain set on the server' },
        host: { label: 'Host', type: 'text', placeholder: 'e.g., 127.0.0.1', defaultValue: 'localhost', tooltip: 'Server address' },
        port: { label: 'Port', type: 'number', placeholder: 'e.g., 1234', defaultValue: '1234', tooltip: 'Server port' }
    }),
    ...device('nmea2000', 'NMEA2000', {
        interface: { label: 'Interface', type: 'text', placeholder: 'CAN interface name', defaultValue: 'can0', tooltip: 'socketCAN interface, Linux only' }
    }),
    ...stored({
        verbose: { label: 'Verbose', type: 'toggle', defaultValue: false, width: 25, tooltip: 'Log message counts' }
    })
};

// The process's standard output: the journal under systemd, the container log
// under Docker. Managed mode keeps it silent unless set here; the levels are
// those of the command line's -o.
export const screenSchema = fields({
    screen: { label: 'Screen Output', type: 'select', defaultValue: '0', tooltip: 'Messages printed to the console or log',
        options: opts(['0', 'Off'], ['1', 'NMEA'], ['2', 'NMEA + details'], ['3', 'JSON with NMEA'], ['4', 'JSON (compact)'], ['5', 'JSON (full)']) },
    verbose_time: { label: 'Verbose Interval (seconds)', type: 'number', defaultValue: 3, min: 1, max: 3600, tooltip: 'How often Verbose logs its counts' }
});

export const webhookSchema = fields({
    description: ChannelFields.description(),
    url: { section: 'Connection', label: 'URL', type: 'text', width: 75, placeholder: 'https://homeassistant.local:8123/api/webhook/ais',
        tooltip: 'Each event is posted here as JSON' },
    active: ChannelFields.active(),
    events: { section: 'Events', label: 'Events', type: 'list', width: 50, defaultValue: [], placeholder: 'safety', tooltip: 'One per line; empty sends every kind' },
    batch: { section: 'Events', label: 'One request for several', type: 'toggle', defaultValue: false, width: 50, tooltip: 'Several events per request' },
    secret: { section: 'Security', label: 'Signing secret', type: 'password', width: 75, placeholder: 'whsec_…', tooltip: 'Signs each request (Standard Webhooks)' }
});

// The station's state on an MQTT broker, for Home Assistant
export const mqttStateSchema = stored({
    url: { section: 'Broker', label: 'Broker', type: 'text', width: 75, placeholder: 'mqtt://homeassistant.local:1883', tooltip: 'mqtt://, mqtts://, ws:// or wss://' },
    username: { section: 'Broker', label: 'User', type: 'text', width: 50 },
    password: { section: 'Broker', label: 'Password', type: 'password', width: 50 },
    topic: { section: 'Topics', label: 'Topic', type: 'text', defaultValue: 'ais-catcher', width: 50, tooltip: 'Prefix of every topic' },
    discovery: { section: 'Topics', label: 'Home Assistant discovery', type: 'toggle', defaultValue: true, width: 50, tooltip: 'Home Assistant finds them by itself' },
    watch: { section: 'Vessels', label: 'Watched vessels', type: 'list', defaultValue: [], width: 50, placeholder: '244123456', tooltip: 'MMSIs to track, one per line' }
});

// In the order the rail, the Outputs pages and Flow list them. `value` is also the configuration's
// section for a kind kept in a list (`list`); `flow`: its name in Flow where shorter; `stats`: the
// types its statistics report under; `add`: what it is for, on Add an output. The community map
// and the screen are one setting each.
export const OUTPUT_TYPES = [
    { value: 'sharing', label: 'Community', schema: sharingSchema, docs: 'configuration/output/community-feed/' },
    { value: 'udp', list: true, label: 'UDP', schema: udpSchema, stats: ['UDP'], essentials: ['host', 'port'],
        add: 'NMEA to another program on the network', docs: 'configuration/output/UDP/' },
    { value: 'http', list: true, label: 'HTTP', schema: httpSchema, stats: ['HTTP'], essentials: ['url'],
        add: 'Post batches to a web address', docs: 'configuration/output/HTTP/' },
    { value: 'mqtt', list: true, label: 'MQTT', schema: mqttSchema, stats: ['MQTT'], essentials: ['url'],
        add: 'Publish to a broker', docs: 'configuration/output/MQTT/' },
    { value: 'tcp', list: true, label: 'TCP Client', flow: 'TCP', schema: tcpSchema, stats: ['TCP Client'], essentials: ['host', 'port'],
        add: 'Connect out to a server and send', docs: 'configuration/output/TCP-client/' },
    { value: 'tcp_listener', list: true, label: 'TCP Server', schema: tcpServerSchema, stats: ['TCP Listener'], essentials: ['port'],
        add: 'Let programs connect in and read', docs: 'configuration/output/TCP-server/' },
    // one kind, three backends: each reports its statistics under its own type
    { value: 'db', list: true, label: 'Database', schema: dbSchema, stats: ['PostgreSQL', 'SQLite', 'CSV'], essentials: ['type', 'conn_str'],
        add: 'SQLite, PostgreSQL or CSV files', docs: 'configuration/output/PSQL/' },
    { value: 'screen', label: 'Screen', schema: screenSchema, add: 'Print the messages in the log', docs: 'configuration/output/console/' }
];
export const outputType = value => OUTPUT_TYPES.find(o => o.value === value);
export const LIST_OUTPUTS = OUTPUT_TYPES.filter(o => o.list);

export function channelTitle(label, item) {
    if (item && item.host && item.port) return label + ' · ' + item.host + ':' + item.port;
    // the label says HTTP or MQTT already: the address without its scheme
    if (item && item.url) return label + ' · ' + String(item.url).replace(/^[a-z][a-z0-9+.-]*:\/\//i, '');
    if (item && item.host) return label + ' · ' + item.host;
    if (item && item.port) return label + ' · :' + item.port;
    return label;
}
