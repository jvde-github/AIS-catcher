// Configuration schemas for AIS-catcher

// HTTP Channel Schema
const httpSchema = {
    protocol: {
        name: 'protocol',
        label: 'Protocol',
        type: 'select',
        required: true,
        defaultValue: 'AISCATCHER',
        options: [
            { value: 'AISCATCHER', label: 'AISCATCHER' },
            { value: 'APRS', label: 'APRS' },
            { value: 'LIST', label: 'LIST' },
            { value: 'AIRFRAMES', label: 'AIRFRAMES' },
            { value: 'NMEA', label: 'NMEA' }
        ],
        width: 75
    },
    interval: {
        name: 'interval',
        label: 'Interval (seconds)',
        type: 'number',
        required: true,
        min: 1,
        defaultValue: 60,
        placeholder: '60',
        width: 25
    },
    url: {
        name: 'url',
        label: 'URL',
        type: 'text',
        required: true,
        placeholder: 'https://example.com'
    },
    id: {
        name: 'id',
        label: 'ID',
        type: 'text',
        required: true,
        placeholder: 'Station ID'
    },
    userpwd: {
        name: 'userpwd',
        label: 'Credentials',
        type: 'text',
        placeholder: 'user:password'
    },
    gzip: {
        name: 'gzip',
        label: 'Gzip',
        type: 'toggle',
        defaultValue: false,
        width: 25
    },
    response: {
        name: 'response',
        label: 'Response',
        type: 'toggle',
        defaultValue: true,
        width: 25
    },
    filter: {
        name: 'filter',
        label: 'Filter',
        type: 'toggle',
        defaultValue: false,
        width: 25
    }
};

// UDP Channel Schema
const udpSchema = {
    host: {
        name: 'host',
        label: 'Host',
        type: 'text',
        required: true,
        defaultValue: '127.0.0.1',
        placeholder: 'e.g., 192.168.1.101',
        width: 75
    },
    port: {
        name: 'port',
        label: 'Port',
        type: 'number',
        required: true,
        defaultValue: 10110,
        placeholder: '10110',
        width: 25
    },
    description: {
        name: 'description',
        label: 'Description',
        type: 'text',
        placeholder: 'Optional description'
    },
    active: {
        name: 'active',
        label: 'Active',
        type: 'toggle',
        defaultValue: true,
        width: 25
    },
    broadcast: {
        name: 'broadcast',
        label: 'Broadcast',
        type: 'toggle',
        defaultValue: false,
        width: 25
    },
    msgformat: {
        name: 'msgformat',
        label: 'Message Format',
        type: 'select',
        defaultValue: 'NMEA',
        options: [
            { value: 'NMEA', label: 'NMEA' },
            { value: 'JSON_NMEA', label: 'JSON with NMEA' },
            { value: 'JSON_FULL', label: 'JSON Full' }
        ]
    }
};

// TCP Channel Schema
const tcpSchema = {
    host: {
        name: 'host',
        label: 'Host',
        type: 'text',
        required: true,
        defaultValue: '127.0.0.1',
        placeholder: 'e.g., 192.168.1.101',
        width: 75
    },
    port: {
        name: 'port',
        label: 'Port',
        type: 'number',
        required: true,
        defaultValue: 10110,
        placeholder: '10110',
        width: 25
    },
    description: {
        name: 'description',
        label: 'Description',
        type: 'text',
        placeholder: 'Optional description'
    },
    active: {
        name: 'active',
        label: 'Active',
        type: 'toggle',
        defaultValue: true,
        width: 25
    },
    persist: {
        name: 'persist',
        label: 'Reconnect',
        type: 'toggle',
        defaultValue: true,
        width: 25
    },
    keep_alive: {
        name: 'keep_alive',
        label: 'Keep Alive',
        type: 'toggle',
        defaultValue: false,
        width: 25
    },
    msgformat: {
        name: 'msgformat',
        label: 'Message Format',
        type: 'select',
        defaultValue: 'NMEA',
        options: [
            { value: 'NMEA', label: 'NMEA' },
            { value: 'JSON_NMEA', label: 'JSON with NMEA' },
            { value: 'JSON_FULL', label: 'JSON Full' }
        ]
    }
};

// TCP Server Schema
const tcpServerSchema = {
    port: {
        name: 'port',
        label: 'Port',
        type: 'number',
        required: true,
        defaultValue: 5010,
        placeholder: '5010'
    },
    description: {
        name: 'description',
        label: 'Description',
        type: 'text',
        placeholder: 'Optional description'
    },
    active: {
        name: 'active',
        label: 'Active',
        type: 'toggle',
        defaultValue: true
    },
    msgformat: {
        name: 'msgformat',
        label: 'Message Format',
        type: 'select',
        defaultValue: 'NMEA',
        options: [
            { value: 'NMEA', label: 'NMEA' },
            { value: 'JSON_NMEA', label: 'JSON with NMEA' },
            { value: 'JSON_FULL', label: 'JSON Full' }
        ]
    }
};

// MQTT Channel Schema
const mqttSchema = {
    url: {
        name: 'url',
        label: 'URL',
        type: 'text',
        required: true,
        placeholder: 'mqtt[s]://[user:pass@]host[:port]'
    },
    topic: {
        name: 'topic',
        label: 'Topic',
        type: 'text',
        required: true,
        placeholder: 'ais/data',
        defaultValue: 'ais/data'
    },
    client_id: {
        name: 'client_id',
        label: 'Client ID',
        type: 'text',
        placeholder: 'Optional client identifier'
    },
    qos: {
        name: 'qos',
        label: 'QoS',
        type: 'select',
        defaultValue: '0',
        options: [
            { value: '0', label: '0 - At most once' },
            { value: '1', label: '1 - At least once' },
            { value: '2', label: '2 - Exactly once' }
        ]
    },
    msgformat: {
        name: 'msgformat',
        label: 'Message Format',
        type: 'select',
        defaultValue: 'JSON_FULL',
        options: [
            { value: 'NMEA', label: 'NMEA' },
            { value: 'JSON_NMEA', label: 'JSON with NMEA' },
            { value: 'JSON_FULL', label: 'JSON Full' }
        ]
    }
};

// Webviewer/Server Schema
const webviewerSchema = {
    port: {
        name: 'port',
        label: 'Port',
        type: 'number',
        jsonpath: 'port',
        defaultValue: 8100,
        min: 1,
        max: 65535,
        width: 25
    },
    station: {
        name: 'station',
        label: 'Station',
        type: 'text',
        jsonpath: 'station',
        defaultValue: 'My Station',
        width: 75
    },
    station_link: {
        name: 'station_link',
        label: 'Station Link',
        type: 'text',
        jsonpath: 'station_link',
        placeholder: 'https://...'
    },
    lat: {
        name: 'lat',
        label: 'Latitude',
        type: 'number',
        jsonpath: 'lat',
        defaultValue: 0.0000,
        step: 0.0001,
        min: -90,
        max: 90,
        width: 50
    },
    lon: {
        name: 'lon',
        label: 'Longitude',
        type: 'number',
        jsonpath: 'lon',
        defaultValue: 0.0000,
        step: 0.0001,
        min: -180,
        max: 180,
        width: 50
    },
    plugin_dir: {
        name: 'plugin_dir',
        label: 'Plugin Directory',
        type: 'text',
        jsonpath: 'plugin_dir',
        placeholder: '/path/to/plugins'
    },
    cdn: {
        name: 'cdn',
        label: 'CDN URL',
        type: 'text',
        jsonpath: 'cdn',
        placeholder: 'https://cdn.example.com'
    },
    file: {
        name: 'file',
        label: 'File',
        type: 'text',
        jsonpath: 'file',
        required: true
    },
    backup: {
        name: 'backup',
        label: 'Backup',
        type: 'number',
        jsonpath: 'backup',
        defaultValue: 10,
        min: 0,
        width: 25
    },
    context: {
        name: 'context',
        label: 'Context',
        type: 'text',
        jsonpath: 'context',
        defaultValue: 'settings',
        width: 75
    },
    active: {
        name: 'active',
        label: 'Active',
        type: 'toggle',
        jsonpath: 'active',
        defaultValue: true,
        width: 24
    },
    share_loc: {
        name: 'share_loc',
        label: 'Show Location',
        type: 'toggle',
        jsonpath: 'share_loc',
        defaultValue: false,
        width: 24
    },
    realtime: {
        name: 'realtime',
        label: 'Realtime',
        type: 'toggle',
        jsonpath: 'realtime',
        defaultValue: true,
        width: 24
    },
    geojson: {
        name: 'geojson',
        label: 'GeoJSON',
        type: 'toggle',
        jsonpath: 'geojson',
        defaultValue: true,
        width: 24
    },
    prome: {
        name: 'prome',
        label: 'Prometheus',
        type: 'toggle',
        jsonpath: 'prome',
        defaultValue: true,
        width: 24
    },
    log: {
        name: 'log',
        label: 'Show Log',
        type: 'toggle',
        jsonpath: 'log',
        defaultValue: true,
        width: 24
    },
    decoder: {
        name: 'decoder',
        label: 'Decoder',
        type: 'toggle',
        jsonpath: 'decoder',
        defaultValue: false,
        width: 24
    }
};

// Sharing Schema
const sharingSchema = {
    sharing: {
        name: 'sharing',
        label: 'Enable Sharing',
        type: 'toggle',
        defaultValue: false
    },
    sharing_key: {
        name: 'sharing_key',
        label: 'Sharing Key (UUID)',
        type: 'text',
        placeholder: 'Enter UUID',
        pattern: '^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$'
    },
    _create_key_button: {
        name: '_create_key_button',
        type: 'button',
        buttonLabel: 'Create New Sharing Key',
        buttonIcon: '<svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 7h8m0 0v8m0-8L10 14" /></svg>',
        onClick: () => window.open('https://aiscatcher.org/register', '_blank'),
        skipSave: true  // Don't save this field to JSON
    }
};

// Static receiverSchema with all device-specific fields pre-merged
const receiverSchema = {
    input: {
        name: "input",
        label: "Device Type",
        type: "select",
        jsonpath: "input",
        options: [
            { value: "", label: "Select Device" },
            { value: "RTLSDR", label: "RTLSDR" },
            { value: "AIRSPY", label: "AIRSPY" },
            { value: "AIRSPYHF", label: "AIRSPYHF" },
            { value: "HACKRF", label: "HACKRF" },
            { value: "SERIALPORT", label: "SERIAL" },
            { value: "UDPSERVER", label: "UDP Server" },
            { value: "RTLTCP", label: "TCP Client" },
            { value: "SPYSERVER", label: "SPYSERVER" },
            { value: "NMEA2000", label: "NMEA2000" }
        ],
        withButton: {
            label: "",
            onClick: "openDeviceSelectionModal",
            icon: `<svg xmlns="http://www.w3.org/2000/svg" height="20px" viewBox="0 -960 960 960" width="20px" fill="currentColor">
                    <path d="M784-120 532-372q-30 24-69 38t-83 14q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l252 252-56 56ZM380-400q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z" />
                </svg>`
        },
        onChange: "clearSerial"
    },
    serial: {
        name: "serial",
        label: "Serial Key",
        type: "text",
        jsonpath: "serial",
        placeholder: "Optional Serial Key",
        dependsOn: {
            field: "input",
            value: ["RTLSDR", "AIRSPY", "AIRSPYHF", "HACKRF"]
        }
    },
    rtlsdr_tuner: {
        name: "tuner",
        label: "Tuner Gain",
        type: "auto-float",  // Special type: accepts "auto" or float within range
        jsonpath: "rtlsdr.tuner",
        min: 0,
        max: 50,
        step: 0.1,
        defaultValue: "auto",  // Can be "auto" or a number
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_bandwidth: {
        name: "rtlsdr_bandwidth",
        label: "Bandwidth",
        type: "integer-select",
        jsonpath: "rtlsdr.bandwidth",
        defaultValue: 0,
        min: 0,
        max: 1000000,
        placeholder: "e.g., 0 (Off), 192000",
        presets: [
            { value: 0, label: "Off" },
            { value: 192000, label: "192K" }
        ],
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_sample_rate: {
        name: "rtlsdr_sample_rate",
        label: "Sample Rate",
        type: "integer-select",
        jsonpath: "rtlsdr.sample_rate",
        defaultValue: 1536000,
        min: 225000,
        max: 3200000,
        placeholder: "e.g., 288000, 1536000",
        presets: [
            { value: 288000, label: "288K" },
            { value: 1536000, label: "1536K (default)" }
        ],
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    rtlsdr_freqoffset: {
        name: "rtlsdr_freqoffset",
        label: "Frequency Offset",
        type: "number",
        jsonpath: "rtlsdr.freqoffset",
        min: -50,
        max: 50,
        defaultValue: 0,
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
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "RTLSDR"
        }
    },
    airspyhf_sample_rate: {
        name: "airspyhf_sample_rate",
        label: "Sample Rate",
        type: "integer-select",
        jsonpath: "airspyhf.sample_rate",
        defaultValue: 192000,
        min: 192000,
        max: 768000,
        placeholder: "e.g., 192000, 256000, 384000, 768000",
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
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "AIRSPYHF"
        }
    },
    airspy_gain_mode: {
        name: "airspy_gain_mode",
        label: "Gain Mode",
        type: "select",
        jsonpath: "airspy.gain_mode",
        defaultValue: "linearity",
        options: [
            { value: "free", label: "Free" },
            { value: "linearity", label: "Linearity" },
            { value: "sensitivity", label: "Sensitivity" }
        ],
        dependsOn: {
            field: "input",
            value: "AIRSPY"
        }
    },
    airspy_vga: {
        name: "airspy_vga",
        label: "VGA",
        type: "number",
        jsonpath: "airspy.vga",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: 10,
        placeholder: "0-14",
        width: 33,
        dependsOn: {
            field: "airspy_gain_mode",
            value: "free"
        }
    },
    airspy_mixer: {
        name: "airspy_mixer",
        label: "Mixer",
        type: "auto-integer",
        jsonpath: "airspy.mixer",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 33,
        dependsOn: {
            field: "airspy_gain_mode",
            value: "free"
        }
    },
    airspy_lna: {
        name: "airspy_lna",
        label: "LNA",
        type: "auto-integer",
        jsonpath: "airspy.lna",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 32,
        dependsOn: {
            field: "airspy_gain_mode",
            value: "free"
        }
    },
    airspy_linearity: {
        name: "airspy_linearity",
        label: "Linearity",
        type: "number",
        jsonpath: "airspy.linearity",
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: "airspy_gain_mode",
            value: "linearity"
        }
    },
    airspy_sensitivity: {
        name: "airspy_sensitivity",
        label: "Sensitivity",
        type: "number",
        jsonpath: "airspy.sensitivity",
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: "airspy_gain_mode",
            value: "sensitivity"
        }
    },
    airspy_biastee: {
        name: "airspy_biastee",
        label: "Bias tee",
        type: "toggle",
        jsonpath: "airspy.biastee",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "AIRSPY"
        }
    },
    rtltcp_protocol: {
        name: "rtltcp_protocol",
        label: "Protocol",
        type: "select",
        jsonpath: "rtltcp.protocol",
        defaultValue: "rtltcp",
        options: [
            { value: "txt", label: "NMEA" },
            { value: "none", label: "Raw" },
            { value: "rtltcp", label: "RTLTCP" },
            { value: "ws", label: "WS" },
            { value: "wsmqtt", label: "WSMQTT" },
            { value: "mqtt", label: "MQTT" },
            { value: "gpsd", label: "GPSD" },
            { value: "beast", label: "BEAST" },
            { value: "basestation", label: "BASESTATION" }
        ],
        dependsOn: {
            field: "input",
            value: "RTLTCP"
        }
    },
    rtltcp_host: {
        name: "rtltcp_host",
        label: "Host",
        type: "text",
        jsonpath: "rtltcp.host",
        placeholder: "e.g., 127.0.0.1",
        dependsOn: {
            field: "input",
            value: "RTLTCP"
        },
        width: 75
    },
    rtltcp_port: {
        name: "rtltcp_port",
        label: "Port",
        type: "number",
        jsonpath: "rtltcp.port",
        placeholder: "e.g., 1234",
        dependsOn: {
            field: "input",
            value: "RTLTCP"
        },
        width: 25
    },
    rtltcp_sample_rate: {
        name: "rtltcp_sample_rate",
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
        type: "number",
        jsonpath: "rtltcp.tuner",
        min: 0,
        max: 50,
        step: 0.1,
        defaultValue: 33,
        placeholder: "0-50, auto",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "rtltcp"
        }
    },
    rtltcp_rtlagc: {
        name: "rtltcp_rtlagc",
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
        label: "Protocols",
        type: "text",
        jsonpath: "rtltcp.protocols",
        defaultValue: "mqtt",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wsmqtt"]
        }
    },
    rtltcp_binary: {
        name: "rtltcp_binary",
        label: "Binary Mode",
        type: "toggle",
        jsonpath: "rtltcp.binary",
        defaultValue: true,
        width: 25,
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wsmqtt"]
        }
    },
    rtltcp_origin: {
        name: "rtltcp_origin",
        label: "Origin",
        type: "text",
        jsonpath: "rtltcp.origin",
        placeholder: "Origin header for WebSocket",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["ws", "wsmqtt"]
        }
    },
    rtltcp_topic: {
        name: "rtltcp_topic",
        label: "Topic",
        type: "text",
        jsonpath: "rtltcp.topic",
        defaultValue: "ais/data",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "wsmqtt"]
        }
    },
    rtltcp_client_id: {
        name: "rtltcp_client_id",
        label: "Client ID",
        type: "text",
        jsonpath: "rtltcp.client_id",
        placeholder: "MQTT client identifier",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "wsmqtt"]
        }
    },
    rtltcp_username: {
        name: "rtltcp_username",
        label: "Username",
        type: "text",
        jsonpath: "rtltcp.username",
        placeholder: "MQTT username",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "wsmqtt"]
        }
    },
    rtltcp_password: {
        name: "rtltcp_password",
        label: "Password",
        type: "text",
        jsonpath: "rtltcp.password",
        placeholder: "MQTT password",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "wsmqtt"]
        }
    },
    rtltcp_qos: {
        name: "rtltcp_qos",
        label: "QoS Level",
        type: "select",
        jsonpath: "rtltcp.qos",
        options: [
            { value: 0, label: "0" },
            { value: 1, label: "1" },
            { value: 2, label: "2" }
        ],
        defaultValue: "0",
        dependsOn: {
            field: "rtltcp_protocol",
            value: ["mqtt", "wsmqtt"]
        }
    },
    serialport_port: {
        name: "serialport_port",
        label: "Port",
        type: "text",
        jsonpath: "serialport.port",
        placeholder: "e.g., /dev/tty0",
        withButton: {
            label: "",
            onClick: "openSerialDeviceModal",
            icon: `<svg xmlns="http://www.w3.org/2000/svg" height="20px" viewBox="0 -960 960 960" width="20px" fill="currentColor">
                        <path d="M784-120 532-372q-30 24-69 38t-83 14q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l252 252-56 56ZM380-400q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z"/>
                    </svg>`
        },
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
        dependsOn: {
            field: "input",
            value: "SERIALPORT"
        }
    },
    serialport_initseq: {
        name: "serialport_init_seq",
        label: "Init Sequence",
        type: "text",
        jsonpath: "serialport.init_seq",
        placeholder: "",
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
        dependsOn: {
            field: "input",
            value: "UDPSERVER"
        },
        width: 75
    },
    udpserver_port: {
        name: "udpserver_port",
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
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "HACKRF"
        }
    },
    spyserver_gain: {
        name: "spyserver_gain",
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
        label: "Port",
        type: "number",
        jsonpath: "spyserver.port",
        placeholder: "e.g., 5555",
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
        dependsOn: {
            field: "input",
            value: "NMEA2000"
        }
    }
};
