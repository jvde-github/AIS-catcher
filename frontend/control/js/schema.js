// The hub's configuration schema: what each section holds, how it is shown and checked.

import { DEVICES, RADIOS } from './inputs.js';

export const ChannelFields = {
    host: () => ({
        name: 'host',
        section: 'Connection',
        label: 'Address',
        type: 'text',
        defaultValue: '127.0.0.1',
        placeholder: 'e.g., 192.168.1.101',
        width: 75
    }),
    port: () => ({
        name: 'port',
        section: 'Connection',
        label: 'Port',
        type: 'number',
        defaultValue: 10110,
        placeholder: '10110',
        width: 25
    }),
    description: () => ({
        name: 'description',
        section: 'Label',
        label: 'Description',
        type: 'text',
        placeholder: 'Optional description',
        width: 50
    }),
    link: () => ({
        name: 'link',
        tooltip: 'Your page at this service, linked from Statistics',
        section: 'Label',
        label: 'Web Page',
        type: 'text',
        placeholder: 'https://…',
        width: 50
    }),
    active: () => ({
        name: 'active',
        section: 'Options',
        label: 'Active',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Pause without removing'
    }),
    unique: () => ({
        name: 'unique',
        section: 'Filter',
        label: 'Unique',
        type: 'toggle',
        defaultValue: false,
        width: 50,
        tooltip: 'Skip repeats within 3 seconds'
    }),
    msgformat: (defaultValue = 'NMEA') => ({
        name: 'msgformat',
        section: 'Format',
        label: 'Message Format',
        type: 'select',
        defaultValue: defaultValue,
        tooltip: 'Format of forwarded messages',
        options: [
            { value: 'NMEA', label: 'NMEA' },
            { value: 'NMEA_TAG', label: 'NMEA + TAG' },
            { value: 'JSON_NMEA', label: 'JSON with NMEA' },
            { value: 'JSON_SPARSE', label: 'JSON (compact)' },
            { value: 'JSON_FULL', label: 'JSON (full)' },
            { value: 'JSON_ANNOTATED', label: 'JSON (annotated)' },
            { value: 'BINARY_NMEA', label: 'Binary' }
        ]
    }),
    position_interval: () => ({
        name: 'position_interval',
        section: 'Filter',
        label: 'Downsample Position',
        type: 'switch-integer',
        tooltip: 'One position per vessel per interval',
        width: 50,
        defaultValue: false,
        defaultInteger: 60,
        min: 0,
        max: 3600,
        step: 1
    })
};

export const sdrGainFields = (prefix, inputValue) => ({
    [`${prefix}_gain_mode`]: {
        name: `${prefix}_gain_mode`,
        label: "Gain Mode",
        type: "select",
        jsonpath: `${prefix}.gain_mode`,
        defaultValue: "linearity",
        options: [
            { value: "free", label: "Free" },
            { value: "linearity", label: "Linearity" },
            { value: "sensitivity", label: "Sensitivity" }
        ],
        tooltip: "Free sets each stage",
        dependsOn: {
            field: "input",
            value: inputValue
        }
    },
    [`${prefix}_vga`]: {
        name: `${prefix}_vga`,
        label: "VGA",
        type: "number",
        jsonpath: `${prefix}.vga`,
        min: 0,
        max: 14,
        step: 1,
        defaultValue: 10,
        placeholder: "0-14",
        width: 33,
        dependsOn: {
            field: `${prefix}_gain_mode`,
            value: "free"
        }
    },
    [`${prefix}_mixer`]: {
        name: `${prefix}_mixer`,
        label: "Mixer",
        type: "auto-integer",
        jsonpath: `${prefix}.mixer`,
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 33,
        dependsOn: {
            field: `${prefix}_gain_mode`,
            value: "free"
        }
    },
    [`${prefix}_lna`]: {
        name: `${prefix}_lna`,
        label: "LNA",
        type: "auto-integer",
        jsonpath: `${prefix}.lna`,
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 32,
        dependsOn: {
            field: `${prefix}_gain_mode`,
            value: "free"
        }
    },
    [`${prefix}_linearity`]: {
        name: `${prefix}_linearity`,
        label: "Linearity",
        type: "number",
        jsonpath: `${prefix}.linearity`,
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: `${prefix}_gain_mode`,
            value: "linearity"
        }
    },
    [`${prefix}_sensitivity`]: {
        name: `${prefix}_sensitivity`,
        label: "Sensitivity",
        type: "number",
        jsonpath: `${prefix}.sensitivity`,
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: `${prefix}_gain_mode`,
            value: "sensitivity"
        }
    },
    [`${prefix}_biastee`]: {
        name: `${prefix}_biastee`,
        label: "Bias tee",
        type: "toggle",
        jsonpath: `${prefix}.biastee`,
        tooltip: "Power an external LNA",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: inputValue
        }
    }
});

export const httpSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    url: {
        name: 'url',
        tooltip: 'Address to post to',
        section: 'Connection',
        label: 'URL',
        type: 'text',
        width: 75,
        placeholder: 'https://example.com'
    },
    interval: {
        name: 'interval',
        section: 'Connection',
        label: 'Interval (seconds)',
        type: 'number',
        min: 1,
        max: 86400,
        defaultValue: 60,
        placeholder: '60',
        width: 25,
        tooltip: 'How often messages are posted'
    },
    active: ChannelFields.active(),
    id: {
        name: 'id',
        section: 'Connection',
        label: 'ID',
        type: 'text',
        width: 25,
        placeholder: 'Station ID',
        tooltip: 'Station identifier in the feed'
    },
    userpwd: {
        name: 'userpwd',
        section: 'Connection',
        label: 'Credentials',
        type: 'text',
        width: 75,
        placeholder: 'user:password',
        tooltip: 'Sent as user:password'
    },
    gzip: {
        name: 'gzip',
        section: 'Options',
        label: 'Gzip',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Compress posted data'
    },
    response: {
        name: 'response',
        section: 'Options',
        label: 'Response',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Log the server response'
    },
    protocol: {
        name: 'protocol',
        section: 'Format',
        label: 'Protocol',
        type: 'select',
        defaultValue: 'AISCATCHER',
        options: [
            { value: 'AISCATCHER', label: 'AISCATCHER' },
            { value: 'MINIMAL', label: 'MINIMAL' },
            { value: 'APRS', label: 'APRS' },
            { value: 'LIST', label: 'LIST' },
            { value: 'AIRFRAMES', label: 'AIRFRAMES' },
            { value: 'NMEA', label: 'NMEA' }
        ],
        tooltip: 'Format the server expects'
    },
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const udpSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    active: ChannelFields.active(),
    broadcast: {
        name: 'broadcast',
        section: 'Options',
        label: 'Broadcast',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Allow broadcast addresses'
    },
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const tcpSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    active: ChannelFields.active(),
    persist: {
        name: 'persist',
        section: 'Options',
        label: 'Reconnect',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Retry after failures'
    },
    keep_alive: {
        name: 'keep_alive',
        section: 'Options',
        label: 'Keep Alive',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Enable TCP keep-alive probes'
    },
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const tcpServerSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    port: {
        name: 'port',
        section: 'Connection',
        label: 'Port',
        type: 'number',
        defaultValue: 5010,
        placeholder: '5010',
        tooltip: 'Port clients connect to, max 64'
    },
    active: ChannelFields.active(),
    msgformat: ChannelFields.msgformat(),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

// One channel for every backend: type picks the class, the rest is shared.
export const dbSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    type: {
        name: 'type',
        tooltip: 'Where messages are stored',
        section: 'Connection',
        label: 'Database',
        type: 'select',
        jsonpath: 'type',
        defaultValue: 'sqlite',
        options: [
            { value: 'sqlite', label: 'SQLite (local file)' },
            { value: 'postgres', label: 'PostgreSQL' },
            { value: 'csv', label: 'CSV files' }
        ],
        width: 40
    },
    conn_str: {
        name: 'conn_str',
        section: 'Connection',
        label: 'Connection',
        type: 'text',
        jsonpath: 'conn_str',
        placeholder: 'ais.db  ·  dbname=ais  ·  /var/log/ais',
        tooltip: 'File, connection string or folder',
        width: 60
    },
    retention: {
        name: 'retention',
        section: 'Retention',
        label: 'Retention (days)',
        type: 'number',
        jsonpath: 'retention',
        min: 0,
        defaultValue: 0,
        tooltip: 'Days kept, 0 for all'
    },
    capacity: {
        name: 'capacity',
        label: 'State Capacity',
        type: 'number',
        jsonpath: 'capacity',
        min: 64,
        max: 1000000,
        defaultValue: 8192,
        advanced: true,
        tooltip: 'CSV: targets kept in the state file',
        dependsOn: {
            field: 'type',
            value: 'csv'
        }
    },
    station_id: {
        name: 'station_id',
        section: 'Station',
        label: 'Station ID',
        type: 'number',
        jsonpath: 'station_id',
        min: 0,
        tooltip: 'Overrides the station on every row'
    },
    active: ChannelFields.active(),
    state: {
        name: 'state',
        section: 'Options',
        label: 'State',
        type: 'toggle',
        jsonpath: 'state',
        defaultValue: true,
        width: 25,
        tooltip: 'Latest values per vessel'
    },
    stats: {
        name: 'stats',
        section: 'Options',
        label: 'Statistics',
        type: 'toggle',
        jsonpath: 'stats',
        defaultValue: true,
        width: 25,
        tooltip: 'Hourly reception statistics'
    },
    position: {
        name: 'position',
        section: 'Options',
        label: 'Positions',
        type: 'toggle',
        jsonpath: 'position',
        defaultValue: false,
        width: 25,
        tooltip: 'Every position report'
    },
    dbstatic: {
        name: 'static',
        section: 'Options',
        label: 'Static',
        type: 'toggle',
        jsonpath: 'static',
        defaultValue: false,
        width: 25,
        tooltip: 'Static and voyage reports'
    },
    nmea: {
        name: 'nmea',
        section: 'Options',
        label: 'Raw NMEA',
        type: 'toggle',
        jsonpath: 'nmea',
        defaultValue: false,
        width: 25,
        tooltip: 'Keep raw sentences'
    },
    interval: {
        name: 'interval',
        tooltip: 'Seconds between writes',
        label: 'Write Interval (seconds)',
        type: 'number',
        jsonpath: 'interval',
        min: 5,
        max: 1800,
        defaultValue: 60,
        advanced: true
    },
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const mqttSchema = {
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    url: {
        name: 'url',
        tooltip: 'Broker address',
        section: 'Connection',
        label: 'URL',
        type: 'text',
        placeholder: 'mqtt[s]://[user:pass@]host[:port]'
    },
    active: ChannelFields.active(),
    topic: {
        name: 'topic',
        section: 'Connection',
        label: 'Topic',
        type: 'text',
        placeholder: 'ais/data',
        defaultValue: 'ais/data',
        tooltip: 'Accepts %mmsi%, %type%, %channel%'
    },
    client_id: {
        name: 'client_id',
        tooltip: 'Name the broker sees',
        section: 'Connection',
        label: 'Client ID',
        type: 'text',
        width: 50,
        placeholder: 'Optional client identifier'
    },
    qos: {
        name: 'qos',
        tooltip: 'Delivery guarantee',
        section: 'Connection',
        label: 'QoS',
        type: 'select',
        width: 50,
        defaultValue: '0',
        options: [
            { value: '0', label: '0 - At most once' },
            { value: '1', label: '1 - At least once' },
            { value: '2', label: '2 - Exactly once' }
        ],
    },
    msgformat: ChannelFields.msgformat('JSON_FULL'),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval()
};

export const sharingSchema = {
    sharing: {
        name: 'sharing',
        label: 'Enable Sharing',
        type: 'toggle',
        defaultValue: false,
        tooltip: 'Share with the community map'
    },
    sharing_key: {
        name: 'sharing_key',
        label: 'Sharing Key (UUID)',
        type: 'text',
        placeholder: 'Enter UUID',
        tooltip: 'Leave empty to share anonymously'
    }
};

// Viewer settings are applied through the versioned configuration feed.
export const receiverSchema = {
    description: {
        name: "description",
        label: "Description",
        type: "text",
        placeholder: "e.g. Mast antenna",
    },
    station: {
        name: "station",
        label: "Station",
        type: "number",
        jsonpath: "station",
        min: 1,
        step: 1,
        placeholder: "its own",
        tooltip: "Where this antenna stands"
    },
    input: {
        name: "input",
        label: "Device Type",
        type: "select",
        jsonpath: "input",

        options: [
            { value: "", label: "Select a device" },
            ...Object.entries(DEVICES).filter(([, d]) => !d.hidden).map(([value, d]) => ({ value, label: d.label || d.name }))
        ],
        // another device: the serial of the one before goes
        clears: ["serial"]
    },
    channel: {
        name: "channel",
        label: "Channel",
        type: "select",
        jsonpath: "channel",
        defaultValue: "AB",
        options: [
            { value: "AB", label: "AB" },
            { value: "CD", label: "CD" }
        ],
        tooltip: "AB standard, CD long range",
        dependsOn: {
            field: "input",
            value: RADIOS
        }
    },
    nmea_channel: {
        name: "nmea_channel",
        label: "Report As",
        type: "select",
        jsonpath: "nmea_channel",
        options: [
            { value: "", label: "CD" },
            { value: "AB", label: "AB" }
        ],
        tooltip: "Channel letters in NMEA output",
        dependsOn: {
            field: "channel",
            value: "CD"
        }
    },
    serial: {
        name: "serial",
        label: "Serial Key",
        type: "text",
        jsonpath: "serial",
        placeholder: "Optional Serial Key",

        tooltip: "Pick a device by serial number",
        dependsOn: {
            field: "input",
            value: RADIOS
        }
    },
    active: {
        name: "active",
        label: "Active",
        type: "toggle",
        jsonpath: "active",
        defaultValue: true,

        width: 25
    },
    engines: {
        name: "engines",
        label: "",
        type: "engines",
        jsonpath: "engines",
        defaultValue: undefined,

        advanced: "Engines",
        options: [
            { value: "auto", label: "Auto" },
            { value: "v1_base", label: "AIS Engine v1 Base" },
            { value: "v1_high", label: "AIS Engine v1 High" },
            { value: "v2_base", label: "AIS Engine v2 Base (experimental)" }
        ],
        tooltip: "A second engine doubles CPU",
        settings: [
            {
                name: "fp_ds",
                label: "Fixed-point downsampling",
                type: "toggle",
                types: ["v1_base", "v1_high", "v2_base"],
                tooltip: "Less CPU on small devices"
            }
        ]
    },
    rtlsdr_tuner: {
        name: "tuner",
        label: "Tuner Gain",
        type: "auto-float",
        jsonpath: "rtlsdr.tuner",
        min: 0,
        max: 50,
        step: 0.1,
        defaultValue: "auto",
        tooltip: "Gain in dB, or auto for AGC",
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_bandwidth: {
        name: "rtlsdr_bandwidth",
        label: "Bandwidth (Hz)",
        type: "integer-select",
        jsonpath: "rtlsdr.bandwidth",
        defaultValue: 0,
        min: 0,
        max: 1000000,
        placeholder: "in Hz, e.g. 0 (Off), 192000 or 192K",
        presets: [
            { value: 0, label: "Off" },
            { value: 192000, label: "192K" }
        ],
        tooltip: "Off uses the device default",
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_sample_rate: {
        name: "rtlsdr_sample_rate",
        label: "Sample Rate (Hz)",
        type: "integer-select",
        jsonpath: "rtlsdr.sample_rate",
        defaultValue: 1536000,
        min: 96000,
        max: 3200000,
        placeholder: "in Hz, e.g. 1536000 or 1536K",
        presets: [
            { value: 288000, label: "288K" },
            { value: 1536000, label: "1536K (default)" }
        ],
        tooltip: "1536K recommended; 288K saves CPU",
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_freqoffset: {
        name: "rtlsdr_freqoffset",
        label: "Frequency Correction (ppm)",
        type: "number",
        jsonpath: "rtlsdr.freqoffset",
        min: -150,
        max: 150,
        defaultValue: 0,
        tooltip: "Frequency correction",
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_biastee: {
        name: "rtlsdr_biastee",
        label: "Bias tee",
        type: "toggle",
        jsonpath: "rtlsdr.biastee",
        tooltip: "Power an external LNA",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_rtlagc: {
        name: "rtlsdr_rtlagc",
        label: "RTLAGC",
        type: "toggle",
        jsonpath: "rtlsdr.rtlagc",
        tooltip: "RTL2832U internal AGC",
        defaultValue: true,
        width: 25,
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    airspyhf_sample_rate: {
        name: "airspyhf_sample_rate",
        tooltip: "Higher costs more CPU",
        label: "Sample Rate (Hz)",
        type: "integer-select",
        jsonpath: "airspyhf.sample_rate",
        defaultValue: 192000,
        min: 192000,
        max: 768000,
        placeholder: "in Hz, e.g. 192000 or 192K",
        presets: [
            { value: 192000, label: "192K" },
            { value: 256000, label: "256K" },
            { value: 384000, label: "384K" },
            { value: 768000, label: "768K" }
        ],
        dependsOn: {
            field: "input",
            value: "AIRSPYHF"
        }
    },
    airspyhf_threshold: {
        name: "airspyhf_threshold",
        label: "Threshold",
        type: "select",
        jsonpath: "airspyhf.threshold",
        defaultValue: "low",
        options: [
            { value: "low", label: "Low" },
            { value: "high", label: "High" }
        ],
        tooltip: "Automatic gain control threshold",
        dependsOn: {
            field: "input",
            value: "AIRSPYHF"
        }
    },
    airspyhf_preamp: {
        name: "airspyhf_preamp",
        label: "Preamp",
        type: "toggle",
        jsonpath: "airspyhf.preamp",
        tooltip: "Enable the built-in preamplifier",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "AIRSPYHF"
        }
    },
    ...sdrGainFields('airspy', 'AIRSPY'),
    ...sdrGainFields('hydrasdr', 'HYDRASDR'),
    rtltcp_protocol: {
        name: "rtltcp_protocol",
        label: "Protocol",
        type: "select",
        jsonpath: "rtltcp.protocol",
        defaultValue: "rtltcp",
        options: [
            { value: "txt", label: "NMEA" },
            { value: "tls", label: "NMEA over TLS" },
            { value: "none", label: "Raw" },
            { value: "rtltcp", label: "RTLTCP" },
            { value: "ws", label: "WS" },
            { value: "wss", label: "WSS" },
            { value: "wsmqtt", label: "WSMQTT" },
            { value: "wssmqtt", label: "WSSMQTT" },
            { value: "mqtt", label: "MQTT" },
            { value: "mqtts", label: "MQTTS" },
            { value: "gpsd", label: "GPSD" },
            { value: "beast", label: "BEAST" },
            { value: "basestation", label: "BASESTATION" },
            { value: "raw1090", label: "RAW1090" }
        ],
        tooltip: "How the server is reached",
        dependsOn: {
            field: "input",
            value: "RTLTCP"
        }
    },
    rtltcp_host: {
        name: "rtltcp_host",
        tooltip: "Server address",
        label: "Host",
        type: "text",
        jsonpath: "rtltcp.host",
        placeholder: "e.g., 127.0.0.1",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["txt", "tls", "none", "rtltcp", "wsmqtt", "wssmqtt", "mqtt", "mqtts", "gpsd", "beast", "basestation", "raw1090"]
        },
        width: 75
    },
    rtltcp_port: {
        name: "rtltcp_port",
        tooltip: "Server port",
        label: "Port",
        type: "number",
        jsonpath: "rtltcp.port",
        placeholder: "e.g., 1234",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["txt", "tls", "none", "rtltcp", "wsmqtt", "wssmqtt", "mqtt", "mqtts", "gpsd", "beast", "basestation", "raw1090"]
        },
        width: 25
    },
    rtltcp_url: {
        name: "rtltcp_url",
        label: "URL",
        type: "text",
        jsonpath: "rtltcp.url",
        placeholder: "wss://host:port/path?query",
        tooltip: "user:password@ or token@ for login",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wss"]
        }
    },
    rtltcp_ssl_verify: {
        name: "rtltcp_ssl_verify",
        tooltip: "Check the server's certificate",
        label: "Verify Certificate",
        type: "toggle",
        jsonpath: "rtltcp.ssl_verify",
        defaultValue: true,
        width: 25,
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["wss", "tls", "mqtts", "wssmqtt"]
        }
    },
    rtltcp_sample_rate: {
        name: "rtltcp_sample_rate",
        tooltip: "Must match the server",
        label: "Sample Rate (Hz)",
        type: "number",
        jsonpath: "rtltcp.sample_rate",
        min: 0,
        max: 20000000,
        defaultValue: 288000,
        placeholder: "0-20,000,000",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        },
        width: 50
    },
    rtltcp_bandwidth: {
        name: "rtltcp_bandwidth",
        tooltip: "0 uses the default",
        label: "Bandwidth (Hz)",
        type: "number",
        jsonpath: "rtltcp.bandwidth",
        min: 0,
        max: 1000000,
        defaultValue: 0,
        placeholder: "0-1,000,000",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        },
        width: 50
    },
    rtltcp_freqoffset: {
        name: "rtltcp_freqoffset",
        tooltip: "Frequency correction",
        label: "Frequency Offset (PPM)",
        type: "number",
        jsonpath: "rtltcp.freqoffset",
        min: -150,
        max: 150,
        defaultValue: 0,
        placeholder: "-150 to +150",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        }
    },
    rtltcp_tuner: {
        name: "rtltcp_tuner",
        label: "Tuner Gain",
        type: "auto-float",
        jsonpath: "rtltcp.tuner",
        min: 0,
        max: 50,
        step: 0.1,
        defaultValue: "auto",
        tooltip: "Gain in dB, or auto for AGC",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        }
    },
    rtltcp_rtlagc: {
        name: "rtltcp_rtlagc",
        tooltip: "Automatic gain on the dongle",
        label: "RTL AGC",
        type: "toggle",
        jsonpath: "rtltcp.rtlagc",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        }
    },
    rtltcp_protocols: {
        name: "rtltcp_protocols",
        tooltip: "Subprotocols to offer the server",
        label: "Protocols",
        type: "text",
        jsonpath: "rtltcp.protocols",
        placeholder: "WebSocket sub-protocols",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wss"]
        }
    },
    rtltcp_binary: {
        name: "rtltcp_binary",
        tooltip: "Server sends binary frames",
        label: "Binary Mode",
        type: "toggle",
        jsonpath: "rtltcp.binary",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wss"]
        }
    },
    rtltcp_origin: {
        name: "rtltcp_origin",
        tooltip: "Origin header to send",
        label: "Origin",
        type: "text",
        jsonpath: "rtltcp.origin",
        placeholder: "Origin header for WebSocket",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wss", "wsmqtt", "wssmqtt"]
        }
    },
    rtltcp_topic: {
        name: "rtltcp_topic",
        tooltip: "Topic to subscribe to",
        label: "Topic",
        type: "text",
        jsonpath: "rtltcp.topic",
        defaultValue: "ais/data",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "mqtts", "wsmqtt", "wssmqtt"]
        }
    },
    rtltcp_client_id: {
        name: "rtltcp_client_id",
        tooltip: "Name the broker sees",
        label: "Client ID",
        type: "text",
        jsonpath: "rtltcp.client_id",
        placeholder: "MQTT client identifier",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "mqtts", "wsmqtt", "wssmqtt"]
        }
    },
    rtltcp_username: {
        name: "rtltcp_username",
        tooltip: "Login name",
        label: "Username",
        type: "text",
        jsonpath: "rtltcp.username",
        placeholder: "Username (MQTT, or HTTP Basic for WebSocket)",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "mqtts", "wsmqtt", "wssmqtt", "ws", "wss"]
        }
    },
    rtltcp_password: {
        name: "rtltcp_password",
        tooltip: "Login password",
        label: "Password",
        type: "text",
        jsonpath: "rtltcp.password",
        placeholder: "Password, or a bearer token with no username",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "mqtts", "wsmqtt", "wssmqtt", "ws", "wss"]
        }
    },
    rtltcp_qos: {
        name: "rtltcp_qos",
        tooltip: "Delivery guarantee",
        label: "QoS Level",
        type: "select",
        jsonpath: "rtltcp.qos",
        options: [
            { value: "0", label: "0" },
            { value: "1", label: "1" },
            { value: "2", label: "2" }
        ],
        defaultValue: "0",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "mqtts", "wsmqtt", "wssmqtt"]
        }
    },
    serialport_port: {
        name: "serialport_port",
        label: "Port",
        type: "text",
        jsonpath: "serialport.port",
        placeholder: "e.g., /dev/tty0",
        dependsOn: {
            field: "input",
            value: "SERIALPORT"
        }
    },
    serialport_baudrate: {
        name: "serialport_baudrate",
        label: "Baud Rate",
        type: "select",
        jsonpath: "serialport.baudrate",
        defaultValue: "38400",
        options: [
            { value: "9600", label: "9600" },
            { value: "19200", label: "19200" },
            { value: "38400", label: "38400" },
            { value: "57600", label: "57600" },
            { value: "115200", label: "115200" }
        ],
        tooltip: "38400 for most AIS receivers",
        dependsOn: {
            field: "input",
            value: "SERIALPORT"
        }
    },
    serialport_init_seq: {
        name: "serialport_init_seq",
        label: "Init Sequence",
        type: "text",
        tooltip: "Sent when the port opens",
        jsonpath: "serialport.init_seq",
        dependsOn: {
            field: "input",
            value: "SERIALPORT"
        }
    },
    serialport_dump_file: {
        name: "serialport_dump_file",
        label: "Dump File",
        type: "text",
        jsonpath: "serialport.dump_file",
        placeholder: "e.g., /var/log/ais-serial.log",
        tooltip: "Write raw data to this file",
        advanced: true,
        dependsOn: {
            field: "input",
            value: "SERIALPORT"
        }
    },
    udpserver_server: {
        name: "udpserver_server",
        label: "Server Address",
        type: "text",
        jsonpath: "udpserver.server",
        placeholder: "e.g., 127.0.0.1",
        tooltip: "Address to listen on",
        dependsOn: {
            field: "input",
            value: "UDPSERVER"
        },
        width: 75
    },
    udpserver_port: {
        name: "udpserver_port",
        tooltip: "Port to listen on",
        label: "Port",
        type: "number",
        jsonpath: "udpserver.port",
        placeholder: "e.g., 1234",
        dependsOn: {
            field: "input",
            value: "UDPSERVER"
        },
        width: 25
    },
    hackrf_lna: {
        name: "hackrf_lna",
        tooltip: "RF gain in dB",
        label: "LNA Gain",
        type: "number",
        jsonpath: "hackrf.lna",
        min: 0,
        max: 40,
        step: 8,
        defaultValue: 8,
        placeholder: "0-40 (in steps of 8)",
        dependsOn: {
            field: "input",
            value: "HACKRF"
        },
        width: 50
    },
    hackrf_vga: {
        name: "hackrf_vga",
        tooltip: "Baseband gain in dB",
        label: "VGA Gain",
        type: "number",
        jsonpath: "hackrf.vga",
        min: 0,
        max: 62,
        step: 2,
        defaultValue: 20,
        placeholder: "0-62 (in steps of 2)",
        dependsOn: {
            field: "input",
            value: "HACKRF"
        },
        width: 50
    },
    hackrf_preamp: {
        name: "hackrf_preamp",
        label: "Preamp",
        type: "toggle",
        jsonpath: "hackrf.preamp",
        tooltip: "Enable the built-in preamplifier",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "HACKRF"
        }
    },
    sdrplay_grdb: {
        name: "sdrplay_grdb",
        label: "Gain Reduction (dB)",
        type: "number",
        jsonpath: "sdrplay.grdb",
        min: 0,
        max: 59,
        step: 1,
        defaultValue: 32,
        placeholder: "0-59",
        tooltip: "Less reduction, more gain",
        dependsOn: { field: "input", value: "SDRPLAY" },
        width: 50
    },
    sdrplay_lnastate: {
        name: "sdrplay_lnastate",
        label: "LNA State",
        type: "number",
        jsonpath: "sdrplay.lnastate",
        min: 0,
        max: 9,
        step: 1,
        defaultValue: 0,
        placeholder: "0-9",
        tooltip: "0 is the most gain",
        dependsOn: { field: "input", value: "SDRPLAY" },
        width: 50
    },
    sdrplay_agc: {
        name: "sdrplay_agc",
        label: "AGC",
        type: "toggle",
        jsonpath: "sdrplay.agc",
        defaultValue: false,
        tooltip: "Automatic gain control",
        dependsOn: { field: "input", value: "SDRPLAY" },
        width: 25
    },
    sdrplay_antenna: {
        name: "sdrplay_antenna",
        label: "Antenna",
        type: "select",
        jsonpath: "sdrplay.antenna",
        options: [
            { value: "A", label: "A" },
            { value: "B", label: "B" },
            { value: "C", label: "C" }
        ],
        tooltip: "The port on an RSP2 or RSPdx",
        dependsOn: { field: "input", value: "SDRPLAY" }
    },
    sdrplay_tuner: {
        name: "sdrplay_tuner",
        label: "Tuner",
        type: "select",
        jsonpath: "sdrplay.tuner",
        options: [
            { value: "A", label: "A" },
            { value: "B", label: "B" }
        ],
        tooltip: "The tuner on an RSPduo",
        dependsOn: { field: "input", value: "SDRPLAY" }
    },
    soapysdr_device: {
        name: "soapysdr_device",
        label: "Device",
        type: "text",
        jsonpath: "soapysdr.device",
        placeholder: "e.g., driver=rtlsdr,serial=00000001",
        tooltip: "SoapySDR's device arguments",
        dependsOn: { field: "input", value: "SOAPYSDR" }
    },
    soapysdr_gain: {
        name: "soapysdr_gain",
        label: "Gain",
        type: "text",
        jsonpath: "soapysdr.gain",
        placeholder: "e.g., TUNER=40",
        tooltip: "Per gain element, as name=value",
        dependsOn: { field: "input", value: "SOAPYSDR" }
    },
    soapysdr_agc: {
        name: "soapysdr_agc",
        label: "AGC",
        type: "toggle",
        jsonpath: "soapysdr.agc",
        defaultValue: true,
        tooltip: "Automatic gain control",
        dependsOn: { field: "input", value: "SOAPYSDR" },
        width: 25
    },
    soapysdr_antenna: {
        name: "soapysdr_antenna",
        label: "Antenna",
        type: "text",
        jsonpath: "soapysdr.antenna",
        tooltip: "The device's antenna by name",
        dependsOn: { field: "input", value: "SOAPYSDR" },
        width: 50
    },
    soapysdr_ch: {
        name: "soapysdr_ch",
        label: "Channel",
        type: "number",
        jsonpath: "soapysdr.ch",
        min: 0,
        max: 32,
        step: 1,
        defaultValue: 0,
        tooltip: "The device's receive channel",
        dependsOn: { field: "input", value: "SOAPYSDR" },
        width: 50
    },
    soapysdr_stream: {
        name: "soapysdr_stream",
        label: "Stream Arguments",
        type: "text",
        jsonpath: "soapysdr.stream",
        placeholder: "name=value,…",
        tooltip: "Passed to the stream as it opens",
        dependsOn: { field: "input", value: "SOAPYSDR" }
    },
    soapysdr_setting: {
        name: "soapysdr_setting",
        label: "Device Settings",
        type: "text",
        jsonpath: "soapysdr.setting",
        placeholder: "name=value,…",
        tooltip: "Written to the device as it opens",
        dependsOn: { field: "input", value: "SOAPYSDR" }
    },
    spyserver_gain: {
        name: "spyserver_gain",
        tooltip: "Gain set on the server",
        label: "Tuner Gain",
        type: "number",
        jsonpath: "spyserver.gain",
        min: 0,
        max: 50,
        step: 0.1,
        defaultValue: 0,
        placeholder: "Gain (0-50)",
        dependsOn: {
            field: "input",
            value: "SPYSERVER"
        }
    },
    spyserver_host: {
        name: "spyserver_host",
        tooltip: "Server address",
        label: "Host",
        type: "text",
        jsonpath: "spyserver.host",
        placeholder: "e.g., 127.0.0.1",
        defaultValue: "localhost",
        dependsOn: {
            field: "input",
            value: "SPYSERVER"
        }
    },
    spyserver_port: {
        name: "spyserver_port",
        tooltip: "Server port",
        label: "Port",
        type: "number",
        jsonpath: "spyserver.port",
        placeholder: "e.g., 1234",
        defaultValue: "1234",
        dependsOn: {
            field: "input",
            value: "SPYSERVER"
        }
    },
    nmea2000_interface: {
        name: "nmea2000_interface",
        label: "Interface",
        type: "text",
        jsonpath: "nmea2000.interface",
        placeholder: "CAN interface name",
        defaultValue: "can0",
        tooltip: "socketCAN interface, Linux only",
        dependsOn: {
            field: "input",
            value: "NMEA2000"
        }
    },
    verbose: {
        name: "verbose",
        label: "Verbose",
        type: "toggle",
        jsonpath: "verbose",
        defaultValue: false,
        width: 25,
        tooltip: "Log message counts"
    }
};

// The process's standard output: the journal under systemd, the container log
// under Docker. Managed mode keeps it silent unless set here; the levels are
// those of the command line's -o.
export const screenSchema = {
    screen: {
        name: 'screen',
        label: 'Screen Output',
        type: 'select',
        defaultValue: '0',
        options: [
            { value: '0', label: 'Off' },
            { value: '1', label: 'NMEA' },
            { value: '2', label: 'NMEA + details' },
            { value: '3', label: 'JSON with NMEA' },
            { value: '4', label: 'JSON (compact)' },
            { value: '5', label: 'JSON (full)' }
        ],
        tooltip: 'Messages printed to the console or log'
    },
    verbose_time: {
        name: 'verbose_time',
        label: 'Verbose Interval (seconds)',
        type: 'number',
        defaultValue: 3,
        min: 1,
        max: 3600,
        advanced: true,
        tooltip: 'How often Verbose logs its counts'
    }
};

// Events posted to an address as they happen, signed when it has a secret
export const webhookSchema = {
    description: ChannelFields.description(),
    url: {
        name: 'url', section: 'Connection', label: 'URL', type: 'text', width: 75,
        placeholder: 'https://homeassistant.local:8123/api/webhook/ais',
        tooltip: 'Each event is posted here as JSON'
    },
    active: ChannelFields.active(),
    events: {
        name: 'events', section: 'Events', label: 'Events', type: 'list', width: 50, defaultValue: [],
        placeholder: 'safety',
        tooltip: 'One per line; empty sends every kind'
    },
    batch: {
        name: 'batch', section: 'Events', label: 'One request for several', type: 'toggle', defaultValue: false, width: 50,
        tooltip: 'Several events per request'
    },
    secret: {
        name: 'secret', section: 'Security', label: 'Signing secret', type: 'password', width: 75,
        placeholder: 'whsec_…',
        tooltip: 'Signs each request (Standard Webhooks)'
    }
};

// The station's state on an MQTT broker, for Home Assistant
export const mqttStateSchema = {
    url: {
        name: 'url', section: 'Broker', label: 'Broker', type: 'text', jsonpath: 'url', width: 75,
        placeholder: 'mqtt://homeassistant.local:1883',
        tooltip: 'mqtt://, mqtts://, ws:// or wss://'
    },
    username: { name: 'username', section: 'Broker', label: 'User', type: 'text', jsonpath: 'username', width: 50 },
    password: { name: 'password', section: 'Broker', label: 'Password', type: 'password', jsonpath: 'password', width: 50 },
    topic: {
        name: 'topic', section: 'Topics', label: 'Topic', type: 'text', jsonpath: 'topic', defaultValue: 'ais-catcher', width: 50,
        tooltip: 'Prefix of every topic'
    },
    discovery: {
        name: 'discovery', section: 'Topics', label: 'Home Assistant discovery', type: 'toggle', jsonpath: 'discovery', defaultValue: true, width: 50,
        tooltip: 'Home Assistant finds them by itself'
    },
    watch: {
        name: 'watch', section: 'Vessels', label: 'Watched vessels', type: 'list', jsonpath: 'watch', defaultValue: [], width: 50,
        placeholder: '244123456',
        tooltip: 'MMSIs to track, one per line'
    }
};

// Every kind of output, in the order the rail, the Outputs pages and Flow list them. `value` names
// it in the rail and Flow, and is its section of the configuration for a kind kept in a list
// (`list`); `label` its name, `flow` its name in Flow where shorter; `stats` the types its
// statistics report under; `essentials` what it needs to work; `add` what it is for, on Add an
// output; `docs` its page. The community map and the screen are one setting each.
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
