# Usage

This page provides examples for basic usage of AIS-catcher across different software defined radios (SDRs) and input/output options.

## Running AIS-catcher

List available devices:

```bash
AIS-catcher -l
```

Start decoding with an RTL-SDR:

```bash
AIS-catcher -d serial_number -v
```

Set sample rate and tuner gain:

```bash
AIS-catcher -s 1536000 -gr tuner 40 rtlagc on  
```

## Input Sources

**RTL-SDR**

Set gain, sample rate, frequency correction

**Airspy** 

Set linearity gain mode, sensitivity, VGA

**File**

Read IQ samples or AIS NMEA logs 

**Network** 

RTL-TCP, SpyServer streams

**NMEA** 

Read from GPS or AIS devices

**SoapySDR**

Support for wide variety of SDRs

## Output Options

**Screen**

NMEA, JSON, verbose modes

**UDP** 

Forward streams to aggregators

**TCP**

Send to visualization tools 

**HTTP**

Post messages to web services

**Database**

Store messages in PostgreSQL

**NMEA2000**

Interface with marine networks  

## Hardware Optimization

Optimize performance on low-powered devices with:

- Reduced sample rate
- Fast downsampling 
- Non-coherent receiver models

See [configuration](configuration.md) guide for execution options and [features](features.md) for additional capabilities.
