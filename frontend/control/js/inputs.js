// The inputs as the panel names and groups them: what each kind of device is, what an input
// needs to work, and its name in the rail, Flow and the Stations page.

// Every kind of input the receiver reads, in the order the Device Type list offers them. `name`: an
// input's default name; `label`: its line in that list, where it differs; `docs`: its page; `hidden`:
// not offered there; `radio`: an SDR on this computer, which Find devices lists, a serial picks and
// the community map takes without a sharing key; `iq`: samples over the network, which the engines
// decode as a radio's. Each kind keeps its settings under its own name in lower case (rtlsdr, serialport).
export const DEVICES = {
    RTLSDR: { name: 'RTL-SDR', docs: 'configuration/input/rtlsdr/', radio: true },
    AIRSPY: { name: 'Airspy', docs: 'configuration/input/airspy/', radio: true },
    AIRSPYHF: { name: 'Airspy HF+', docs: 'configuration/input/airspyhf/', radio: true },
    HACKRF: { name: 'HackRF', docs: 'configuration/input/hackrf/', radio: true },
    HYDRASDR: { name: 'HydraSDR', docs: 'configuration/input/hydrasdr/', radio: true },
    SDRPLAY: { name: 'SDRplay', docs: 'configuration/input/sdrplay/', radio: true },
    SOAPYSDR: { name: 'SoapySDR', docs: 'configuration/input/soapysdr/', radio: true },
    SERIALPORT: { name: 'Serial', label: 'Serial (NMEA receiver)', docs: 'configuration/input/serial/' },
    UDPSERVER: { name: 'UDP', label: 'UDP (NMEA over the network)', docs: 'configuration/input/udp/' },
    RTLTCP: { name: 'Network', label: 'Network client (RTL-TCP, TCP, MQTT)', docs: 'configuration/input/tcp/', iq: true },
    SPYSERVER: { name: 'SpyServer', docs: 'configuration/input/spyserver/', iq: true },
    NMEA2000: { name: 'NMEA 2000', docs: 'configuration/input/NMEA2000/' },
    WAVFILE: { name: 'WAV file', docs: 'configuration/input/wav/', hidden: true },
    FILE: { name: 'File', docs: 'configuration/input/file/', hidden: true },
    ZMQ: { name: 'ZMQ', hidden: true }
};

// an input's kind as the table has it: a hand-written "udpserver" is UDPSERVER
const kindOf = input => String(input || '').toUpperCase();
export const deviceOf = input => DEVICES[kindOf(input)];
export const isRadio = input => !!deviceOf(input)?.radio;
// the inputs the engines decode, and a serial tells one apart
export const decodes = input => !!(deviceOf(input)?.radio || deviceOf(input)?.iq);
export const RADIOS = Object.keys(DEVICES).filter(isRadio);
// the settings blocks of a receiver, one per kind
export const DEVICE_BLOCKS = Object.keys(DEVICES).map(k => k.toLowerCase());
export const inputDocs = input => deviceOf(input)?.docs || 'configuration/input/overview/';

// what an input needs to work: the device, which one, and how to reach it; the rest is Advanced
export const INPUT_ESSENTIALS = new Set(['input', 'serial', 'serialport_port', 'serialport_baudrate', 'rtltcp_protocol', 'rtltcp_host', 'rtltcp_port', 'rtltcp_url',
    'udpserver_server', 'udpserver_port', 'spyserver_host', 'spyserver_port', 'nmea2000_interface', 'soapysdr_device']);

// the receiver's fields carry no section: group them by what they are about
export function inputGroup(f) {
    const n = f.name;
    if (n === 'station') return 'Station';
    if (n === 'engines' || n === 'fp_ds') return 'Engines';
    if (n === 'channel' || n === 'nmea_channel') return 'Channels';
    if (n === 'verbose' || n === 'screen' || n === 'verbose_time') return 'Logging';
    if (n.startsWith('timeout')) return 'Timeouts';
    if (n.startsWith('serialport_')) return 'Serial';
    if (n.startsWith('rtltcp_') || n.startsWith('udpserver_')) return 'Connection';
    if (n.startsWith('rtlsdr_') || n === 'tuner' || n.startsWith('airspyhf_') || n.startsWith('hackrf_') || n.startsWith('spyserver_') ||
        n.startsWith('sdrplay_') || n.startsWith('soapysdr_')) return 'Tuning';
    return 'Other';
}

// An input's name: its description, or else the device with its place among the inputs of that
// device ("RTL-SDR #1", "Serial #1"), `all` being the inputs; `kind` the device
export function describeInput(item, i, all) {
    const kind = kindOf(item.input);
    const device = kind ? (deviceOf(kind)?.name || item.input) : '';
    const nth = (all || []).slice(0, i + 1).filter(r => kindOf(r.input) === kind).length || 1;
    const name = (item.description || '').trim();
    return { title: name || (kind ? `${device} #${nth}` : 'New input'), kind: device };
}
