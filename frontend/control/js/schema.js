
const ChannelFields = {
    host: () => ({
        name: 'host',
        label: 'Host',
        type: 'text',
        required: true,
        defaultValue: '127.0.0.1',
        placeholder: 'e.g., 192.168.1.101',
        width: 75
    }),
    port: () => ({
        name: 'port',
        label: 'Port',
        type: 'number',
        required: true,
        defaultValue: 10110,
        placeholder: '10110',
        width: 25
    }),
    description: () => ({
        name: 'description',
        label: 'Description',
        type: 'text',
        placeholder: 'Optional description',
        width: 50
    }),
    link: () => ({
        name: 'link',
        label: 'Link',
        type: 'text',
        placeholder: 'Optional link, e.g. https://example.com',
        width: 50
    }),
    active: () => ({
        name: 'active',
        label: 'Active',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Turn off to pause this channel without removing it'
    }),
    unique: () => ({
        name: 'unique',
        label: 'Unique',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Drop duplicate messages received within a few seconds'
    }),
    msgformat: (defaultValue = 'NMEA') => ({
        name: 'msgformat',
        label: 'Message Format',
        type: 'select',
        defaultValue: defaultValue,
        tooltip: 'Format of forwarded messages: raw NMEA, NMEA with TAG block, or decoded JSON',
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
        label: 'Downsample Position',
        type: 'switch-integer',
        tooltip: 'At most one position report per vessel per interval (seconds); other message types pass unchanged',
        defaultValue: false,
        defaultInteger: 60,
        min: 0,
        max: 3600,
        step: 1
    }),
    zones: () => ({
        name: 'zone',
        label: 'Zones',
        type: 'zones',
        defaultValue: [],
        hint: 'No zones — receives from all inputs'
    })
};

const httpSchema = {
    protocol: {
        name: 'protocol',
        label: 'Protocol',
        type: 'select',
        required: true,
        defaultValue: 'AISCATCHER',
        options: [
            { value: 'AISCATCHER', label: 'AISCATCHER' },
            { value: 'MINIMAL', label: 'MINIMAL' },
            { value: 'APRS', label: 'APRS' },
            { value: 'LIST', label: 'LIST' },
            { value: 'AIRFRAMES', label: 'AIRFRAMES' },
            { value: 'NMEA', label: 'NMEA' }
        ],
        width: 75,
        tooltip: 'Submission format expected by the server, e.g. AISCATCHER JSON or APRS for aprs.fi'
    },
    interval: {
        name: 'interval',
        label: 'Interval (seconds)',
        type: 'number',
        required: true,
        min: 1,
        max: 86400,
        defaultValue: 60,
        placeholder: '60',
        width: 25,
        tooltip: 'How often collected messages are posted to the URL'
    },
    url: {
        name: 'url',
        label: 'URL',
        type: 'text',
        required: true,
        placeholder: 'https://example.com'
    },
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    active: ChannelFields.active(),
    id: {
        name: 'id',
        label: 'ID',
        type: 'text',
        required: true,
        placeholder: 'Station ID',
        tooltip: 'Station identifier included in the feed'
    },
    userpwd: {
        name: 'userpwd',
        label: 'Credentials',
        type: 'text',
        placeholder: 'user:password',
        tooltip: 'HTTP authentication, sent as user:password'
    },
    gzip: {
        name: 'gzip',
        label: 'Gzip',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Compress posted data'
    },
    response: {
        name: 'response',
        label: 'Response',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Log the server response'
    },
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval(),
    zones: ChannelFields.zones()
};

const udpSchema = {
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    active: ChannelFields.active(),
    broadcast: {
        name: 'broadcast',
        label: 'Broadcast',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Allow sending to broadcast addresses'
    },
    unique: ChannelFields.unique(),
    msgformat: ChannelFields.msgformat(),
    position_interval: ChannelFields.position_interval(),
    zones: ChannelFields.zones()
};

const tcpSchema = {
    host: ChannelFields.host(),
    port: ChannelFields.port(),
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    active: ChannelFields.active(),
    persist: {
        name: 'persist',
        label: 'Reconnect',
        type: 'toggle',
        defaultValue: true,
        width: 25,
        tooltip: 'Reconnect automatically after failures'
    },
    keep_alive: {
        name: 'keep_alive',
        label: 'Keep Alive',
        type: 'toggle',
        defaultValue: false,
        width: 25,
        tooltip: 'Enable TCP keep-alive probes'
    },
    unique: ChannelFields.unique(),
    msgformat: ChannelFields.msgformat(),
    position_interval: ChannelFields.position_interval(),
    zones: ChannelFields.zones()
};

const tcpServerSchema = {
    port: {
        name: 'port',
        label: 'Port',
        type: 'number',
        required: true,
        defaultValue: 5010,
        placeholder: '5010',
        tooltip: 'Local port where clients connect to read the stream (max 64 clients)'
    },
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    active: ChannelFields.active(),
    unique: ChannelFields.unique(),
    msgformat: ChannelFields.msgformat(),
    position_interval: ChannelFields.position_interval(),
    zones: ChannelFields.zones()
};

const mqttSchema = {
    url: {
        name: 'url',
        label: 'URL',
        type: 'text',
        required: true,
        placeholder: 'mqtt[s]://[user:pass@]host[:port]'
    },
    description: ChannelFields.description(),
    link: ChannelFields.link(),
    active: ChannelFields.active(),
    topic: {
        name: 'topic',
        label: 'Topic',
        type: 'text',
        required: true,
        placeholder: 'ais/data',
        defaultValue: 'ais/data',
        tooltip: 'Supports placeholders such as %mmsi%, %type% and %channel% for dynamic topics'
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
        ],
    },
    msgformat: ChannelFields.msgformat('JSON_FULL'),
    unique: ChannelFields.unique(),
    position_interval: ChannelFields.position_interval(),
    zones: ChannelFields.zones()
};

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
        width: 75,
        tooltip: 'Station name shown in the web viewer'
    },
    station_link: {
        name: 'station_link',
        label: 'Station Link',
        type: 'text',
        jsonpath: 'station_link',
        placeholder: 'https://...',
        tooltip: 'External website linked from the station name'
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
        placeholder: '/path/to/plugins',
        width: 50,
        tooltip: 'Directory with .pjs/.pss plugins injected into the viewer'
    },
    webcontrol_http: {
        name: 'webcontrol_http',
        label: 'Web Control Link',
        type: 'text',
        jsonpath: 'webcontrol_http',
        defaultValue: '',
        placeholder: 'http://127.0.0.1:8110',
        width: 50,
        tooltip: 'URL of this Web Control, linked from the viewer menu'
    },
    file: {
        name: 'file',
        label: 'Backup File',
        type: 'text',
        jsonpath: 'file',
        required: true,
        width: 75,
        tooltip: 'File where statistics and plot history are saved across restarts'
    },
    backup: {
        name: 'backup',
        label: 'Minutes',
        type: 'number',
        jsonpath: 'backup',
        defaultValue: 10,
        min: 5,
        max: 2880,
        width: 25,
        tooltip: 'How often the statistics file is written'
    },
    history: {
        name: 'history',
        label: 'Ship Timeout (s)',
        type: 'number',
        jsonpath: 'history',
        defaultValue: 1800,
        min: 5,
        max: 43200,
        width: 50,
        tooltip: 'Ships without messages for this long are removed from the viewer'
    },
    context: {
        name: 'context',
        label: 'Context',
        type: 'text',
        jsonpath: 'context',
        defaultValue: 'settings',
        width: 50,
        tooltip: 'Browser storage key for viewer settings; use distinct values to keep multiple viewers separate'
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
        label: 'Location',
        type: 'toggle',
        jsonpath: 'share_loc',
        defaultValue: false,
        width: 24,
        tooltip: 'Show the station location and range on the map'
    },
    use_gps: {
        name: 'use_gps',
        label: 'Use GPS',
        type: 'toggle',
        jsonpath: 'use_gps',
        defaultValue: true,
        width: 24,
        tooltip: 'Let GPS input update the station location'
    },
    realtime: {
        name: 'realtime',
        label: 'Realtime',
        type: 'toggle',
        jsonpath: 'realtime',
        defaultValue: false,
        width: 24,
        tooltip: 'Stream live NMEA messages to the viewer'
    },
    geojson: {
        name: 'geojson',
        label: 'GeoJSON',
        type: 'toggle',
        jsonpath: 'geojson',
        defaultValue: false,
        width: 24,
        tooltip: 'Enable the GeoJSON API endpoints'
    },
    prome: {
        name: 'prome',
        label: 'Prometheus',
        type: 'toggle',
        jsonpath: 'prome',
        defaultValue: false,
        width: 24,
        tooltip: 'Serve Prometheus metrics at /metrics'
    },
    log: {
        name: 'log',
        label: 'Show Log',
        type: 'toggle',
        jsonpath: 'log',
        defaultValue: false,
        width: 24,
        tooltip: 'Show the log tab in the viewer'
    },
    decoder: {
        name: 'decoder',
        label: 'Decoder',
        type: 'toggle',
        jsonpath: 'decoder',
        defaultValue: false,
        width: 24,
        tooltip: 'Enable the NMEA decoder tab'
    },
    zones: {
        name: 'zone',
        label: 'Zones',
        type: 'zones',
        jsonpath: 'zone',
        defaultValue: [],
        hint: 'No zones — receives from all inputs'
    }
};

const sharingSchema = {
    sharing: {
        name: 'sharing',
        label: 'Enable Sharing',
        type: 'toggle',
        defaultValue: false,
        tooltip: 'Share received messages with the aiscatcher.org community map'
    },
    sharing_key: {
        name: 'sharing_key',
        label: 'Sharing Key (UUID)',
        type: 'text',
        placeholder: 'Enter UUID',
        tooltip: 'Station key from aiscatcher.org; leave empty to share anonymously',
        pattern: '^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$'
    },
    _create_key_button: {
        name: '_create_key_button',
        type: 'button',
        buttonLabel: (dataContext) => {
            const sharingKey = dataContext.sharing_key || '';
            return sharingKey.trim() === '' ? 'Create New Sharing Key' : 'Edit Station Details';
        },
        buttonIcon: '<svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 7h8m0 0v8m0-8L10 14" /></svg>',
        onClick: 'openSharingManagement',
        skipSave: true
    },
    sharing_zone: {
        name: 'sharing_zone',
        label: 'Zones',
        type: 'zones',
        defaultValue: [],
        hint: 'No zones — receives from all inputs'
    }
};

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
            { value: "HYDRASDR", label: "HydraSDR" },
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
        tooltip: "AB is the standard AIS channel pair (161.975/162.025 MHz), CD the long-range channels",
        dependsOn: {
            field: "input",
            value: ["RTLSDR", "AIRSPY", "AIRSPYHF", "HACKRF", "HYDRASDR"]
        }
    },
    serial: {
        name: "serial",
        label: "Serial Key",
        type: "text",
        jsonpath: "serial",
        placeholder: "Optional Serial Key",
        tooltip: "Select a specific device by serial number when several are connected",
        dependsOn: {
            field: "input",
            value: ["RTLSDR", "AIRSPY", "AIRSPYHF", "HYDRASDR", "HACKRF"]
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
    sensitivity_high: {
        name: "sensitivity_high",
        label: "High Sensitivity",
        type: "toggle",
        jsonpath: "sensitivity_high",
        defaultValue: false,
        width: 25,
        tooltip: "Adds a higher-sensitivity decoder model for a few percent more messages at extra CPU load",
        dependsOn: {
            field: "input",
            value: ["RTLSDR", "AIRSPY", "AIRSPYHF", "HACKRF", "HYDRASDR"]
        }
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
        tooltip: "Tuner filter bandwidth; Off uses the device default",
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
        tooltip: "1536K is recommended; 288K reduces CPU load on small devices",
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
        tooltip: "Correct the dongle frequency error in ppm",
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
        tooltip: "Power an external LNA over the antenna cable",
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
        tooltip: "Linearity and Sensitivity use one combined gain; Free sets LNA, mixer and VGA individually",
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
        tooltip: "Power an external LNA over the antenna cable",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "AIRSPY"
        }
    },
    hydrasdr_gain_mode: {
        name: "hydrasdr_gain_mode",
        label: "Gain Mode",
        type: "select",
        jsonpath: "hydrasdr.gain_mode",
        defaultValue: "linearity",
        options: [
            { value: "free", label: "Free" },
            { value: "linearity", label: "Linearity" },
            { value: "sensitivity", label: "Sensitivity" }
        ],
        tooltip: "Linearity and Sensitivity use one combined gain; Free sets LNA, mixer and VGA individually",
        dependsOn: {
            field: "input",
            value: "HYDRASDR"
        }
    },
    hydrasdr_vga: {
        name: "hydrasdr_vga",
        label: "VGA",
        type: "number",
        jsonpath: "hydrasdr.vga",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: 10,
        placeholder: "0-14",
        width: 33,
        dependsOn: {
            field: "hydrasdr_gain_mode",
            value: "free"
        }
    },
    hydrasdr_mixer: {
        name: "hydrasdr_mixer",
        label: "Mixer",
        type: "auto-integer",
        jsonpath: "hydrasdr.mixer",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 33,
        dependsOn: {
            field: "hydrasdr_gain_mode",
            value: "free"
        }
    },
    hydrasdr_lna: {
        name: "hydrasdr_lna",
        label: "LNA",
        type: "auto-integer",
        jsonpath: "hydrasdr.lna",
        min: 0,
        max: 14,
        step: 1,
        defaultValue: "auto",
        placeholder: "auto or 0-14",
        width: 32,
        dependsOn: {
            field: "hydrasdr_gain_mode",
            value: "free"
        }
    },
    hydrasdr_linearity: {
        name: "hydrasdr_linearity",
        label: "Linearity",
        type: "number",
        jsonpath: "hydrasdr.linearity",
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: "hydrasdr_gain_mode",
            value: "linearity"
        }
    },
    hydrasdr_sensitivity: {
        name: "hydrasdr_sensitivity",
        label: "Sensitivity",
        type: "number",
        jsonpath: "hydrasdr.sensitivity",
        min: 0,
        max: 21,
        step: 1,
        defaultValue: 17,
        placeholder: "0-21",
        dependsOn: {
            field: "hydrasdr_gain_mode",
            value: "sensitivity"
        }
    },
    hydrasdr_biastee: {
        name: "hydrasdr_biastee",
        label: "Bias tee",
        type: "toggle",
        jsonpath: "hydrasdr.biastee",
        tooltip: "Power an external LNA over the antenna cable",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "input",
            value: "HYDRASDR"
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
            { value: "basestation", label: "BASESTATION" },
            { value: "raw1090", label: "RAW1090" }
        ],
        tooltip: "Stream format of the remote server: raw I/Q (RTLTCP), NMEA text, MQTT, WebSocket, GPSD or ADS-B",
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
        placeholder: "WebSocket sub-protocols",
        dependsOn: {
            field: "rtltcp_protocol",
            value: "ws"
        }
    },
    rtltcp_binary: {
        name: "rtltcp_binary",
        label: "Binary Mode",
        type: "toggle",
        jsonpath: "rtltcp.binary",
        defaultValue: false,
        width: 25,
        dependsOn: {
            field: "rtltcp_protocol",
            value: "ws"
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
            { value: "0", label: "0" },
            { value: "1", label: "1" },
            { value: "2", label: "2" }
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
        tooltip: "38400 is the standard rate for AIS equipment",
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
        tooltip: "Commands sent to the device when the port is opened",
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
        tooltip: "Local address the UDP server listens on",
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
        tooltip: "Enable the built-in preamplifier",
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
        tooltip: "Print received messages and statistics in the log"
    },
    zones: {
        name: "zone",
        label: "Zones",
        type: "zones",
        jsonpath: "zone",
        defaultValue: []
    }
};

const generalSettingsSchema = {
    timeout: {
        name: 'timeout',
        label: 'Timeout (seconds)',
        type: 'off-number',
        jsonpath: 'timeout',
        defaultValue: false,
        defaultNumber: 60,
        min: 1,
        max: 3600,
        step: 1,
        unit: 's',
        tooltip: 'Stop the receiver after this many seconds'
    },
    timeout_only_when_idle: {
        name: 'timeout_only_when_idle',
        label: 'Only When Idle',
        type: 'toggle',
        jsonpath: 'timeout_only_when_idle',
        defaultValue: false,
        width: 50,
        tooltip: 'Count the timeout only while no messages arrive — a watchdog for stalled input'
    }
};
