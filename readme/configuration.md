 Here is a suggested configuration section page:

# Configuration

This page covers configuring AIS-catcher for different use cases via command line options, environment variables, and configuration files.

## Command Line Options

Set input device and output modes directly:

```bash
AIS-catcher -d serial_number -u 192.168.1.100 -p correction  
```

See `AIS-catcher -h` for full list of options.

## Environment Variables

Specify configuration in environment:

```bash
export AISCATCHER_DEVICE=rtlsdr
export AISCATCHER_UDP_HOST=192.168.1.100
```

## Configuration File

Declare options in JSON file:

```json
{
  "input": "rtlsdr",
  "rtlsdr": {
    "ppm": -60,
    "tuner": 40
  },
  "udp": [
    {
      "host": "192.168.1.100"
    }
  ]
}
```

Load configuration:

```bash 
AIS-catcher -C myconfig.json
```

## Usage Profiles

**Receiver**

Minimal options for decoding and output 

**Aggregator** 

Route and filter streams to aggregators

**Database**

Log all messages to PostgreSQL 

**Experimenter**

Simultaneous SDRs, decoder testing

**Web Viewer**

Display vessels via built-in web interface

See [features](features.md) for more advanced capabilities.
