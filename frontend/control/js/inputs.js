// The inputs as the panel names and groups them: what each kind of device is, what an input
// needs to work, and its name in the rail, Flow and the Stations page.

// what an input needs to work: the device, which one, and how to reach it; the rest is Advanced
export const INPUT_ESSENTIALS = new Set(['input', 'serial', 'serialport_port', 'serialport_baudrate', 'rtltcp_protocol', 'rtltcp_host', 'rtltcp_port', 'rtltcp_url',
    'udpserver_server', 'udpserver_port', 'spyserver_host', 'spyserver_port', 'nmea2000_interface']);

// the inputs that are radios: they decode with engines, and a serial tells one apart
export const SDR_INPUTS = new Set(['RTLSDR', 'AIRSPY', 'AIRSPYHF', 'HACKRF', 'HYDRASDR', 'SPYSERVER', 'RTLTCP', 'SDRPLAY', 'SOAPYSDR']);

const INPUT_DESCRIPTIONS = {
    RTLSDR: 'An RTL-SDR dongle on this computer.',
    AIRSPY: 'An Airspy receiver on this computer.',
    AIRSPYHF: 'An Airspy HF+ receiver on this computer.',
    HACKRF: 'A HackRF on this computer.',
    HYDRASDR: 'A HydraSDR on this computer.',
    SERIALPORT: 'An AIS receiver connected by USB or serial cable.',
    UDPSERVER: 'NMEA received over the network by UDP.',
    RTLTCP: 'A radio or feed reached over the network.',
    SPYSERVER: 'A remote radio shared by SpyServer.',
    NMEA2000: 'An NMEA 2000 network interface.'
};

// the device as people write it, for an input's default name
const DEVICE_NAMES = { RTLSDR: 'RTL-SDR', AIRSPY: 'Airspy', AIRSPYHF: 'Airspy HF+', HACKRF: 'HackRF', HYDRASDR: 'HydraSDR',
    SERIALPORT: 'Serial', UDPSERVER: 'UDP', RTLTCP: 'Network', SPYSERVER: 'SpyServer', NMEA2000: 'NMEA 2000' };

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
    if (n.startsWith('rtlsdr_') || n === 'tuner' || n.startsWith('airspyhf_') || n.startsWith('hackrf_') || n.startsWith('spyserver_')) return 'Tuning';
    return 'Other';
}

// A serial port as people know it: a by-id link names the device itself
// ("usb-Raspberry_Pi_Pico_0E451E025C5FA275-if00" is a Raspberry Pi Pico), any other port its last part
export function serialName(path) {
    const base = String(path || '').split(/[\\/]/).pop();
    if (!/^(usb|pci|platform)-/.test(base) || !/[\\/]by-id[\\/]/.test(path)) return base;
    const parts = base.replace(/^[a-z]+-/, '').replace(/-if\d+(-port\d+)?$/, '').split('_');
    // the last part is the device's own serial when it carries a digit
    if (parts.length > 1 && /\d/.test(parts[parts.length - 1]) && parts[parts.length - 1].length >= 4) parts.pop();
    return parts.join(' ') || base;
}

// An input's name: its description, or else the device with its place among the inputs of that
// device ("RTL-SDR #1", "Serial #1"), `all` being the inputs. Underneath: the device and what tells
// it apart, the dongle's serial or the receiver on the port ("Serial · Raspberry Pi Pico").
export function describeInput(item, i, all) {
    // the kind as the schema has it: a hand-written "udpserver" is UDPSERVER
    const kind = String(item.input || '').toUpperCase();
    const device = kind ? (DEVICE_NAMES[kind] || item.input) : '';
    const list = all || [];
    const nth = list.slice(0, i + 1).filter(r => String(r.input || '').toUpperCase() === kind).length || 1;
    const name = (item.description || '').trim();
    const path = kind === 'SERIALPORT' ? String(item.serialport?.port || '').trim() : '';
    const id = path ? serialName(path) : SDR_INPUTS.has(kind) && item.serial ? item.serial : '';
    return {
        title: name || (kind ? `${device} #${nth}` : 'New input'),
        kind: device,
        sub: kind ? (id ? `${device} · ${id}` : device) : '',
        desc: INPUT_DESCRIPTIONS[kind] || 'Pick the device this input reads from.'
    };
}
