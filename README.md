# AIS-catcher: A multi-platform AIS Receiver

This repository presents the `AIS-catcher` software, a versatile dual-channel AIS receiver that is compatible with a wide range of Software Defined Radios (SDRs). These include RTL-SDR dongles (such as the ShipXplorer AIS dongle and RTL SDR Blog v4), AirSpy (Mini/R2/HF+), HackRF, SDRPlay, SoapySDR, and file/network input (ZMQ/RTL-TCP/SpyServer). AIS-catcher delivers output in the form of NMEA messages, which can be conveniently displayed on screen or forwarded via UDP/HTTP/TCP. Designed as a lightweight command line utility, AIS-catcher also incorporates a built-in web server for internal use within secure networks. The project home page including several realtime examples can be found at [aiscatcher.org](https://aiscatcher.org).

# License

Copyright (C) 2021 - 2024 jvde.github at gmail.com. All rights reserved. Licensed under GNU General Public License v3.0.

# Important Disclaimer
`AIS-catcher` is created for research and educational purposes under the GNU GPL v3 license. It is a hobby project and has not been tested and designed for reliability and correctness. 
You can play with the software but it is the user's responsibility to use it prudently. So, DO NOT rely upon this software in any way including for navigation 
and/or safety of life or property purposes.
There are variations in the legislation concerning radio reception in the different administrations around the world. 
It is your responsibility to determine whether or not your local administration permits the reception and handling of AIS messages from ships. 
It is specifically forbidden to use this software for any illegal purpose whatsoever. 
Only use this software in regions where such use is permitted.

# Purpose

The purpose of `AIS-catcher` is to serve as a platform that encourages the perpetual enhancement of receiver models. We greatly value and appreciate any suggestions, observations, or shared recordings, particularly from setups where the existing models encounter difficulties.

# The aiscatcher.org community

AIS-catcher is a free, open-source project aimed at transforming SDR-equipped computers into AIS-receivers. It's continuously improved with decoding enhancements, user support, and expanded output options for commercial data aggregators. As a seperate project we also aggregate AIS data with the aim of real-time visualization in local web dashboards, enhancing receiver performance and situational awareness by integrating nearby station data.

To join, ensure you're on the latest version, visit [aiscatcher.org](https://aiscatcher.org), and [add](https://aiscatcher.org/addstation) your station. Upon registration, you'll receive a personal sharing key. Simply run AIS-catcher on the command line with "-X" followed by your sharing key to share your station's raw AIS data with the community hub. This activates a "Community Feed" in your station's web viewer, accessible under map layers and some other features.

Check the data we're receiving at [aiscatcher.org](https://aiscatcher.org). We welcome your innovative ideas for enhancing AIS-catcher with this collective data, which could lead to new features or improvements benefiting the entire community.
  
# General Installation

Windows [Binaries](https://github.com/jvde-github/AIS-catcher/blob/main/README.md#Build-process) and Building [instructions](https://github.com/jvde-github/AIS-catcher/blob/main/README.md#Build-process) for many systems are provided below. Pre-built Docker container images containing AIS-catcher are [available](https://github.com/jvde-github/AIS-catcher#container-images) from the GitHub Container Registry. 

> **Note:** Issues have been reported for the latest Windows version on Windows 10 where the [VC runtime libraries](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) was not up to date. So please ensure you have the latest VC runtime installed.

In the next chapters we show quick starter guides for setting up AIS-catcher on the Raspberry Pi with an easy web interface for configuration. We also discuss a full installation script for easy install on Debian-based systems. 

# Quick Guide on turning a Raspberry Pi into a AIS receiver

This tutorial will guide you through the process of installing and running AIS-catcher on a Raspberry Pi. AIS-catcher is a software tool used to receive and decode Automatic Identification System (AIS) messages from ships, using Software Defined Radio (SDR) devices like the RTL-SDR dongle.

## Table of Contents
- [Setting Up the Raspberry Pi](#setting-up-the-raspberry-pi)
- [Installing AIS-catcher](#installing-ais-catcher)
- [Configuring AIS-catcher via the Web GUI](#configuring-ais-catcher-via-the-web-gui)
- [Accessing the AIS Web Viewer](#accessing-the-ais-web-viewer)
- [Conclusion](#conclusion)

## Setting Up the Raspberry Pi

The first step in setting up the AIS receiver is preparing your Raspberry Pi if not already done. Begin by downloading and installing the Raspberry Pi Imager from the [official website](https://www.raspberrypi.com/software/). Once installed, launch the Raspberry Pi Imager application. In the application, you'll need to select your preferred Raspberry Pi OS version and choose the SD card you've inserted as your storage device.

Before writing the image, access the advanced options by clicking the settings gear icon. Here, you'll need to configure several important settings. Enable SSH access to allow remote connections to your Pi. Set up a hostname for your device (for example, 'zerowh') and create your username and password credentials. Don't forget to configure your Wi-Fi settings by entering your network's SSID and password.

After configuring these settings, insert the SD card into your Raspberry Pi and power it on. Once the system has booted, you can connect to it via SSH using a terminal. For example, if you used the hostname 'zerowh' and username 'jasper', you would connect using:

```bash
ssh jasper@zerowh
```
For Windows systems you could use [https://www.putty.org/](PuTTy). 

## Installing AIS-catcher

Installing AIS-catcher is straightforward using the provided installation script. Open your terminal and run the following command:

```bash
sudo bash -c "$(wget -qO- https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)"
```

This installation script performs several important tasks. It installs AIS-catcher along with its configuration files, sets up a system service to run AIS-catcher in the background, compiles the program with optimizations for your specific device, and installs necessary drivers from source for compatibility with devices like the RTL-SDR V4.

> **Note:** Be patient during the installation process, as compilation can take considerable time on less powerful devices.

To verify that the installation was successful, you can run the following command:
```bash
AIS-catcher -h
```

## Configuring AIS-catcher via the Web GUI

AIS-catcher provides a web-based graphical user interface for easy configuration. It needs to be installed as a separate service by entering in the terminal:
```bash
sudo bash -c "$(curl -fsSL https://raw.githubusercontent.com/jvde-github/AIS-catcher-control/main/install_ais_catcher_control.sh)"
```
To access it, open your web browser and navigate to your Raspberry Pi's IP address on port 8110 (for example, `http://zerowh:8110`). 

![image](https://github.com/user-attachments/assets/1fe942d2-dd3a-4116-99e8-f88f2de4ed14)


When you first access the interface, use the default credentials (username: `admin`, password: `admin`). You'll be prompted to change this password immediately for security purposes.

![image](https://github.com/user-attachments/assets/aea1eb3b-0344-47a9-8b4f-8ddd77ecdeb0)


### Input Device Selection

In the Input section of the web GUI, you'll need to select your input device. The interface allows you to select from any connected devices or manually specify a device type and serial number. If you're using a single SDR device, you can leave the device selection as 'None', and AIS-catcher will automatically use the available device. You can also click the Search Icon to let AIS-catcher detect the available SDR hardware.

![image](https://github.com/user-attachments/assets/83cc4f88-7d76-49db-b126-e62a2b652663)

Specific device settings for your SDR or other input device can be set on this page as well. 

> **Note:** After any modification to the settings the changes need to be saved and the program needs to be (re)started for the changes to become effective.
> 
### Output Settings

AIS-catcher offers the ability to share your data with the aiscatcher.org community. Navigate to the Output > Community section to enable this feature. By default, sharing is anonymous, but you can generate and enter a sharing key to associate the data with your station and view statistics. If you want to create a sharing key click "Create" and this will take you to the page on aiscatcher.org where you can set up a station and receive a sharing key.

![image](https://github.com/user-attachments/assets/9f032bea-784f-465a-93f4-003a5fd7f587)

#### Local Webviewer

The local web viewer configuration can be found under Output > Web Viewer. Here, you should activate the viewer and enter your station details, including a name and your geographical coordinates.

![image](https://github.com/user-attachments/assets/c59cf7cf-d20b-4c4d-8fa3-e5ba5e1e5471)


This local webviewer is available from your Raspberry device (e.g. in this example at port 8100, hence can be accessed with `http://zerowh:8100` in the browser) and not by default accessible outside the local network. Some users share their webviewer externally, see [here](https://aiscatcher.org/dashboards) for examples. 

> **Note:** If you desire a public page with your station performance the easiest approach is to feed aiscatcher.org with a sharing key.

### Service Control

The Control section is where you manage the AIS-catcher service. Here you can start and stop the service, enable auto-start functionality, and monitor the service status through the log display. 
![image](https://github.com/user-attachments/assets/abf29893-0567-4b94-9354-e0630cc6f9fc)


## Accessing the AIS Web Viewer

Press start in the Control section and ensure that it is running without errors (see the log). Once AIS-catcher is running, you can view your received AIS data through the aforementioned local web viewer. Access it by navigating to your Raspberry Pi's IP address on port 8100 (for example, `http://zerowh:8100`). The viewer provides a real-time display of AIS messages and vessel positions, allowing you to verify that your setup is working correctly. Another option is to have a quick view by choosing the Webviewer menu item in the menu.

![image](https://github.com/user-attachments/assets/1762ee88-e8b0-47a3-b50a-1ca2a7b42acb)

## Conclusion

With these steps completed, you now have a fully functional AIS receiving station running on your Raspberry Pi. The system will receive AIS messages from nearby vessels and, if configured, share this data with the AIScatcher.org community. You can monitor vessel traffic in real-time through the web viewer interface.

For advanced users who want to fine-tune their setup, AIS-catcher provides two configuration files:

The JSON configuration file at:
```bash
/etc/AIS-catcher/config.json
```

And the command-line parameters file at:
```bash
/etc/AIS-catcher/config.cmd
```

> **Note:** The GUI script can also be run for existing installations that are based on the AIS-catcher install script. But once configuration files are manually edited they cannot be edited via the HTML forms anymore. The configuration files still can be edited though under the advanced options menu. 

## References:
- [AIS-catcher GitHub Repository](https://github.com/jvde-github/AIS-catcher)
- [Raspberry Pi Official Website](https://www.raspberrypi.com/)

> **Note:** Always ensure your Raspberry Pi and AIS-catcher software are up to date to benefit from the latest features and security updates.



# Basic Installation on a Raspberry Pi/Ubuntu/Debian Systems

This guide provides instructions for installing AIS-catcher on Debian-based systems (like Raspberry Pi) and setting it up to run as a background service. This ensures AIS-catcher will automatically start when the machine is booted.
## Installation

To install AIS-catcher via a script, open a terminal or log in via SSH, then run the following command:
```console
sudo bash -c "$(wget -qO- https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)"
```
The script will install all dependencies and build AIS-catcher. The required SDR libraries are installed from the official packages if they cannot be found on the system. For the RTL-SDR we build from source from the official package to guarantee support for the RTL-SDR V4 but, again, only if the package is not already installed on the system. On a fresh Raspberry Pi4 this will take less than 20 minutes. 

To update AIS-catcher to the latest version, simply run the above command again.

If you want to use pre-installed Debian packes in the installation use:
```console
sudo bash -c "$(wget -qO- https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)" _ -p
```
The advantage that this avoids an compilation step which can save quite a bit of time on older Raspberry devices but it does not optimize the binaries for the specific hardware and ***is not compatible with the RTL-SDR V4***.

### Verifying the installation

To verify that AIS-catcher is installed or updated, run:
```console
/usr/bin/AIS-catcher -L
```
Now (re)connect the dongle and you can start playing with the various command line options, e.g. to start some basic decoding with a webviewer at `http://localhost:8100`, use file following command:
```console
/usr/bin/AIS-catcher -v 10 -N 8100
```
If all works, you should start seeing NMEA lines on screen and have an active webviewer at the aforementioned address.

### Configuration

For running AIS-catcher as a background service we can use two configuration files:

- /etc/AIS-catcher/config.json (JSON configuration)
- /etc/AIS-catcher/config.cmd (command line parameters)

The simplest approach is to edit the configuration file /etc/AIS-catcher/config.cmd to capture your settings which are detailed below. Lines starting with # are considered comments and ignored. The default file contains comments for popular options, which can be modified using a text editor, for example:
```console
sudo nano /etc/AIS-catcher/config.cmd
```

### Running AIS-catcher as a Background Service

To start AIS-catcher as a background service use the following command:
```console
sudo systemctl start ais-catcher.service
```
To view the status of the service copy the following command:
```console
sudo systemctl status ais-catcher.service
```
To ensure AIS-catcher starts automatically at boot time, enable the service with:
```console
sudo systemctl enable ais-catcher.service
```
### Feedback
This is fairly new script and under development so any feedback is appreciated. 

![image](https://github.com/user-attachments/assets/1be6abdb-7df2-4f4b-8d73-1740e0476013)

## What's new?
**Edge**:
- Options to automatically show the vessel track when hovering over and/or selecting a vessel
- Fix to system daemon file to restart process after 10s only
- Shipcard is placed near the vessel if the screen dimensions allow for it
- Shipcard is draggable
  
**v0.60** version:
- Option to send fully decoded AIS messages in JSON format via UDP and TCP (similar to screen output with `-o 5`). Add `JSON_FULL on` to `~P/S/u`.
- Bug fix for connecting serial devices in macOS
- Option to select columns in the table (and many more options added):
  
  ![image](https://github.com/user-attachments/assets/a89120dc-9a2b-485c-b6ff-5516385ad8ae)

- Option to use text description in tables for ship type
- Overlay in AIS-catcher webviewer that shows possibility of atmospheric ducting conditions enable long range AIS reception
  
  ![image](https://github.com/jvde-github/AIS-catcher/assets/52420030/d15920f1-d827-42da-9f05-3587fe553feb)

- Added debian packages and installation script


Earlier versions:

- Added a measure tool to measure distance and bearing between two points and/or ships. Start with CTRL-Click or activate the measure tool and press +
- Additional option  for `-f`-switch to either append NMEA lines to file (`-f filename MODE APP` for appending - default) or starting fresh with `-f filename MODE OUT`
- Community feed by running with -X. This will feed the aiscatcher.org server and return data as an extra map layer option in your webviewer.
- Performance improvements when drawing a large number of vessels by switching from Leaflet to Openlayers (notice this might require re-working some plugins). in openlayers it is more straightforward to plot ship icons on the canvas.
- Option to auto terminate the program if no messages received for a while, e.g. after 10 minutes `-T 600 nomsg_ony`. This will help cure network connections for input or devices going stale without an error 
- You can access  geoJSON output of the current ship positions by visiting the web viewer at `/geojson` and for KML output, please navigate to `/kml` (enable with the
switches `-N geojson on` and `-N kml on`). The KML feature facilitates the visualization of ship positions in Google Earth Pro. Be sure to add a network link and configure the auto-refresh rate in GE. A demonstration of the use of GeoJSON is [plotting the vessels on the tar1090 map.](https://github.com/jvde-github/AIS-in-TAR1090)
- Experimenter Mode for NMEA2000 via socketCAN on Linux. [See documentation below](https://github.com/jvde-github/AIS-catcher/blob/main/README.md#nmea2000-input-and-output-via-socketcan).
- Pyssel blog post describes procedure to show offline mbtiles maps [here](https://pysselilivet.blogspot.com/2023/12/ais-receiver-and-dispatcher-best.html)

## Usage

AIS-catcher began as a basic decoder for RTL-SDR dongles, offering on-screen output and UDP transmission for key aggregation sites. Over time, we've expanded its compatibility to include a wider range of SDRs and input methods. On the output side, it now supports viewing signals and positions through a web viewer, saving to databases, and forwarding as NMEA2000 on Linux systems using socketCAN. This enhancement has subtly shifted AIS-catcher's role, making it a useful tool for managing different AIS data streams. Below is a cheatsheet for the various input and output modes.

![image](https://github.com/jvde-github/AIS-catcher/assets/52420030/53fdef5c-f8ff-4040-a505-2af06c03c234)

### Detailed settings

````
use: AIS-catcher [options]

        [-a xxx - set tuner bandwidth in Hz (default: off)]
        [-b benchmark demodulation models for time - for development purposes (default: off)]
        [-c [AB/CD] - [optional: AB] select AIS channels and optionally the NMEA channel designations]
        [-C [filename] - read configuration settings from file]
        [-D [connection string] - write messages to PostgreSQL database]
        [-e [baudrate] [serial port] - read NMEA from serial port at specified baudrate]
        [-f [filename] write NMEA lines to file]
        [-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]
        [-h display this message and terminate (default: false)]
        [-H [optional: url] - send messages via HTTP, for options see documentation]
        [-i [interface] - read NMEA2000 data from socketCAN interface - Linux only]
        [-I [interface] - push messages as NMEA2000 data to a socketCAN interface - Linux only]
        [-m xx - run specific decoding model (default: 2), see README for more details]
        [-M xxx - set additional meta data to generate: T = NMEA timestamp, D = decoder related (signal power, ppm) (default: none)]
        [-n show NMEA messages on screen without detail (-o 1)]
        [-N [optional: port][optional settings] - start http server at port, see README for details]
        [-o set output mode (0 = quiet, 1 = NMEA only, 2 = NMEA+, 3 = NMEA+ in JSON, 4 JSON Sparse, 5 JSON Full (default: 2)]
        [-O MMSI - sets the own mmsi of the receiver]
        [-p xxx - set frequency correction for device in PPM (default: zero)]
        [-P xxx.xx.xx.xx yyy - TCP destination address and port (default: off)]
        [-q suppress NMEA messages to screen (-o 0)]
        [-s xxx - sample rate in Hz (default: based on SDR device)]
        [-S xxx - TCP server for NMEA lines at port xxx]
        [-T xx - auto terminate run with SDR after xxx seconds (default: off)]
        [-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]
        [-v [option: xx] - enable verbose mode, optional to provide update frequency of xx seconds (default: false)]
        [-X connect to AIS community feed at aiscatcher.org (default: off)]

        Device selection:

        [-d:x - select device based on index (default: 0)]
        [-d xxxx - select device based on serial number]
        [-e baudrate port - open device at serial port with given baudrate]
        [-l list available devices and terminate (default: off)]
        [-L list supported SDR hardware and terminate (default: off)]
        [-r [optional: yy] filename - read IQ data from file or stdin (.), short for -r -ga FORMAT yy FILE filename
        [-t [[protocol]] [host [port]] - read IQ data from remote RTL-TCP instance]
        [-w filename - read IQ data from WAV file, short for -w -gw FILE filename]
        [-x [server][port] - UDP input of NMEA messages at port on server
        [-y [host [port]] - read IQ data from remote SpyServer]
        [-z [optional [format]] [optional endpoint] - read IQ data from [endpoint] in [format] via ZMQ (default: format is CU8)]

        Device specific settings:

        [-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8] ]
        [-ge Serial Port: PRINT [on/off]
        [-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off] ]
        [-gh Airspy HF+: TRESHOLD [low/high] PREAMP [on/off] ]
        [-gm Airspy: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]
        [-gr RTLSDRs: TUNER [auto/0.0-50.0] RTLAGC [on/off] BIASTEE [on/off] ]
        [-gs SDRPLAY: GRDB [0-59] LNASTATE [0-9] AGC [on/off] ]
        [-gt RTLTCP: HOST [address] PORT [port] TUNER [auto/0.0-50.0] RTLAGC [on/off] FREQOFFSET [-150-150] PROTOCOL [none/rtltcp] TIMEOUT [1-60] ]
        [-gu SOAPYSDR: DEVICE [string] GAIN [string] AGC [on/off] STREAM [string] SETTING [string] CH [0+] PROBE [on/off] ANTENNA [string] ]
        [-gw WAV file: FILE [filename] ]
        [-gy SPYSERVER: HOST [address] PORT [port] GAIN [0-50] ]
        [-gz ZMQ: ENDPOINT [endpoint] FORMAT [CF32/CS16/CU8/CS8] ]

        Model specific settings:

        [-go Model: AFC_WIDE [on/off] FP_DS [on/off] PS_EMA [on/off] SOXR [on/off] SRC [on/off] DROOP [on/off] ]
````

### Basic usage


To test that the installation was successful (see below for instructions), a good start is the following command which lists the devices available for AIS reception:
```console
AIS-catcher -l
```
The output depends on the available devices but will look something like:
```console
Found 1 device(s):
0: Realtek, RTL2838UHIDIR, SN: 00000001
```
A specific device can be selected with the ``d``-switch using the device number ``-d:0`` or the serial number ``-d 00000001``. If you were expecting  particular devices that are missing, you might want to try:
```console
AIS-catcher -L
```
This lists all devices for which support is included in the executable. If particular hardware is not listed here, you might have to install the necessary libraries and rebuild AIS-catcher.

To start AIS decoding, print some occasional statistics (every 10 seconds)
```console
AIS-catcher -v 10
```
The next step is to share the data with other programs or services. 
To share your raw feed with other AIS-catcher users (and see their data in your webviewer - see below) use `-X`. Additionally, for sending the messages via UDP to ports 10110 and 10111, we can use the following command:
```console
AIS-catcher -v 10 -X -u 127.0.0.1 10110 -u 127.0.0.1 10111
```
If successful, NMEA messages will start to come in, appear on the screen and send as UDP messages to `127.0.0.1` port `10110` and port `10111` and will be visible on [aiscatcher.org](https://aiscatcher.org). The UDP messages are the primary method to forward messages for visualization in OpenCPN or to AIS aggregator websites like MarineTraffic, FleetMon, VesselFinder, ShipXplorer and others. See below for more pointers on how this can be set up.
The AIS NMEA lines on screen can be suppressed with the option ```-q```. 

For RTL-SDR devices performance can be sensitive to the device settings. In general, a good starting point is the following:
```console
AIS-catcher -gr RTLAGC on TUNER auto -a 192K
```
It has been reported by several users that adding a bandwidth setting of ``-a 192K`` can be beneficial so it is worthwhile to try with and without this filter. Finding the best settings for your hardware requires some systematic experimentation whereby one parameter is changed at a time, e.g. switch RTLAGC ``on`` or ``off``, set the TUNER to ``auto`` or try fixed tuner gains between 0 and 50. The hardware settings available depend on the hardware and more details can be found below.

AIS-catcher also supports the 18 Euro RPI Zero W. However, the hardware might not keep up with the high data flow. This can sometimes be resolved by activating **fast downsampling** via:
```console
AIS-catcher -F
```
Fast downsampling uses approximations and comes at a very small performance degradation, so is not set by default. If your device still struggles, you can try running at a sample rate of 288K (`-s 288K`):
```console
AIS-catcher -s 288K
```
Reception will be impacted though. Unfortunately, latest feedback seems to be that this is best way to run on the Zero W as this Zero is struggling with the high data throuugput. Another drawback of these lower cost boards is that they can create interference that impacts the radio reception.

Finally, to create a webviewer that you can access from your local network, use the following command:
```console
AIS-catcher -N 8100
```
A simple webviewer with a map (and community feed) will be available at `http://localhost:8100`. The webviewer can be accessed from any device on the same network with the IP address of the machine. The webviewer can be customized, see below.

That's all there is. Below we will dive into some more details.

## Deep dives
![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/containership.jpg)

### Output of messages to screen

The output of NMEA messages to screen can be regulated with the ``-o`` switch. To suppress any messages to screen use ``-o 0`` or ``-q``. This can be useful if you run AIS-catcher as a background process. To show only simple and pure NMEA lines, use the switch ``-o 1`` or ``-n``. Example output:
```
!AIVDM,1,1,,B,33L=LN051HQj3HhRJd7q1W=`0000,0*03
```
By default, and using the command ``-o 2``, AIS-catcher displays NMEA messages with some additional information:
```
!AIVDM,1,1,,B,33L=LN051HQj3HhRJd7q1W=`0000,0*03 ( MSG: 3, REPEAT: 0, MMSI: 230907000, signalpower: -44.0, ppm: 0, timestamp: 20220729191340)
```
This same information but wrapped in JSON to facilitate further processing downstream is generated with the switch ```-o 3``` :
```json
{"class":"AIS","device":"AIS-catcher","channel":"B","rxtime":"20220729191502","signalpower":-44.0,"ppm":0,"mmsi":230907000,"type":3,"nmea":["!AIVDM,1,1,,B,33L=LN051HQj3HhRJd7q1W=`0000,0*03"]}
```
And finally, full decoding of the AIS message is activated via ``-o 5``:
```json
{"class":"AIS","device":"AIS-catcher","rxtime":"20220729191610","scaled":true,"channel":"B","nmea":["!AIVDM,1,1,,B,33L=LN051HQj3HhRJd7q1W=`0000,0*03"],"signalpower":-44.0,"ppm":0.000000,"type":3,"repeat":0,"mmsi":230907000,"status":0,"status_text":"Under way using engine","turn":18,"speed":8.800000,"accuracy":true,"lon":24.915239,"lat":60.148106,"course":231.000000,"heading":230,"second":52,"maneuver":0,"raim":false,"radio":0}
```

Meta data is not calculated by default to keep the program as light as possible when running as a server on low spec devices but can be activated with the ```-M``` switch. The calculation of signal power (in dB) and applied frequency correction (in ppm) are activated with  ``-M D``. NMEA messages are timestamped with  ``-M T`` and additional country information from the station derived from the MMSI is included in JSON output with ``-M M``. 

There are many libraries for decoding AIS messages in NMEA format to JSON format. I encourage you to use your favorite library. Some excellent choices include [libais](https://github.com/schwehr/libais), [gpsdecode](https://github.com/ukyg9e5r6k7gubiekd6/gpsd/blob/master/gpsdecode.c) and [pyais](https://github.com/M0r13n/pyais).

### Web Viewer

![image](https://github.com/jvde-github/AIS-catcher/assets/52420030/54eea1c6-2f72-4c23-91c4-dd289753d4cc)

AIS-catcher includes a simple web interface. A live demo is available for [East Boston, US](https://kx1t.com/ais/). The web interface gratefully uses the following libraries: [chart.js](https://www.chartjs.org/docs/latest/charts/line.html), chart.js [annotation plugin](https://www.chartjs.org/chartjs-plugin-annotation/latest/), [leaflet](https://leafletjs.com/), [Material Design Icons](https://m3.material.io/styles/icons/overview), tabulator, [marked](https://github.com/markedjs/marked) and [flag-icons](https://github.com/lipis/flag-icons). 

Make sure you use the latest version and start the web viewer as follows:
```console
AIS-catcher -N 8100
```
where ``8100`` is the port number. If your machine network name is raspberrypi, e.g.,  then enter ``raspberrypi:8100`` in your browser.  On the web page, you will find several sections with information related to the station and received messages.

For users wishing to include a station name and a link to an external website in the Statistics section:
```console
AIS-catcher -N STATION Southwood STATION_LINK http://example.com
```
This could be a useful option if you want to offer the interface externally. To display the reception range and distances from your station, provide the program with the station coordinates and permission to share the location with the web viewer:
```console
AIS-catcher -N LAT 50 LON 3.141592 SHARE_LOC on
```
The last option `share_loc` (default is off) will allow the web viewer to access and display the location.

 The user can make a page in [markdown format](https://www.markdownguide.org/basic-syntax/). The content will be shown in the About tab of the web viewer:
```console
AIS-catcher -N 8100 ABOUT about.md
```
All these options can be captured in the configuration file (in a section with name ``server``), see below. 

#### Menu structure

The main menu behind the hamburger icon in the top left corner navigates between different functional areas. Context-sensitive menus, accessible through right-click, long press on iOS, or the vertical dot icon on the map, offer more functionalities. Here you can set options like activating the "dark mode" theme, displaying the station range on the map, locking/unlocking the map center, toggling text-only ship labels, decluttering ship labels, and viewing details of the last message received from a vessel, amongst others.

#### Visualization

When AIS-catcher receives data containing a vessel's dimensions but can not determine the direction it is pointing (heading), it will display a circle that accommodates the ship's dimensions regardless of heading. Missing heading information is common for Class B ships. If there's a decent approximation available for the heading, such as course-over-ground above certain speeds, it will be used. Shapes plotted using this approximation will have a dashed border, indicating incomplete information. An example is the USS Constitution docked in Boston.

<p align="center">
  <img src="https://user-images.githubusercontent.com/52420030/219856857-e0965190-1468-47b6-88ad-423b77c455ff.png" width="50%"/>
</p>

In the map section, clicking on a vessel will open a  **ship card** with details of the vessel. For smaller screens it can be minimized in the top bar (via the `^` symbol or by clicking on the header bar). The ship card will open minimized on mobile devices. In its maximized form, users can choose which rows will be visible in the minimized state. Additional options, such as looking up the vessel on aggregator sites, are available by clicking the three-dot icon on the ship card header.

#### Validation
The web-interface shows a "validation" indication at the left border of the ship card header.
<p align="center">
  <img src="https://user-images.githubusercontent.com/52420030/212470486-8987fa96-5324-41d8-a782-dbcbdc18aca0.png" width="25%"/>
</p>

AIS-catcher analyzes an enormous stream of bits per day for both AIS channels (2 to the power 33 to be precise). To avoid erroneous messages, the AIS system employs a 16-bit CRC and matching of other bit patterns. Unfortunately, purely statistically this cannot prevent that there will be an occasional technically correct but nonsense message. These are typically easy to recognize (e.g. looking at the signal level, and location on the map) and aggregator sites like MarineTraffic will filter these out. 

To reliably measure the reception range for the station in the web interface, AIS-catcher has implemented a "validation function" that checks the location of the vessel for consistency between messages and flags if there is an inconsistency. Practically speaking, if we receive a position from an MMSI that is relatively close to the last received position, the "validation" indicator will be green and the distance to the station will be included to determine the station range. Please note that messages within 50 NMi from the receiving station will always be included for range setting. The validation indicator will be grey if validation for the location cannot be performed and red if it is not successful. 

#### Plots
The Plot section contains several visualizations to assess the performance of the receiver:

<p align="center">
  <img src="https://user-images.githubusercontent.com/52420030/219856922-33404fe8-dc54-4bc2-a1a6-84f4ce5dd72a.png" width="50%"/>
</p>
 Restarting AIS-catcher typically erases history in the graphs. To retain plot "state" and backup the information to a file use the following:

```console
AIS-catcher -N 8100 FILE stat.bin BACKUP 10
```
This will back up the plots when the program closes and every 10 minutes in a file `stat.bin`. The minimum backup interval is 5 minutes.

#### Custom plugins and styles...

To give the user the option to tweak the look-and-feel and functionality of the web viewer and/or modify for example the color scheme or regional preferences, the program provides the option to inject custom plugins (JavaScript) and CSS into the website, with a command like:
```console
AIS-catcher -N 8100 PLUGIN plugin1.js PLUGIN plugin2.js STYLE mystyle.css
```
You can also include all plugin files from a specific directory using the command:
```console
AIS-catcher -N 8100 PLUGIN_DIR /usr/share/aiscatcher/plugins
```
Files need to have the extension ``.pjs`` and ``.pss`` for respectively JavaScript and CSS style plugins. The repository includes a few example plugins that demonstrate how to add additional maps or cater to regional preferences. Examples of plugins can be found in [another](https://github.com/jvde-github/AIS-Catcher-PLUGINS) GitHub repository.

#### Offline web viewer
There is an option to run the web viewer without relying on online libraries. This facilitates using the web interface whilst traveling without an internet connection. The steps are simple. First, go to your home directory (say `/home/jasper`) and clone the necessary offline web assets:
```console
git clone https://github.com/jvde-github/webassets.git
```
This will create a directory `webassets` that we need to share with AIS-catcher as an alternative location for online web content  with the CDN argument followed by the location of the web assets directory:
```console
AIS-catcher -x 192.168.1.120 4002 -N 8100 CDN /home/jasper/webassets
```

#### Sending data to Prometheus for use in Grafana dashboards

You can add the option `PROME on` to the web configuration command to start rendering Prometheus-compatible statistics at `/metrics`. For example:

```console
AIS-catcher -N 8100 PROME on
```

For more information on how to configure Prometheus and Grafana to get an initial dashboard, see [README-grafana.md](Documentation/README-grafana.md).

### Posting messages over HTTP

Some cloud services collecting AIS data prefer messages to be periodically posted via the HTTP protocol, for example, [APRS.fi](https://aprs.fi). As per version 0.29, AIS-catcher can do this directly via the ``-H`` switch. For example:
```console
AIS-catcher -r posterholt.raw -v 60 -H http://localhost:8000 INTERVAL 10 ID MyStation
```
will post JSON with the following layout every 10 seconds:

```json
{
	"protocol": "jsonaiscatcher",
	"encodetime": "20221102171325",
	"stationid": "MyStation",
	"receiver":
		{
		"description": "AIS-catcher v0.39",
		"version": 39,
		"engine": "Base (non-coherent)",
		"setting": "droop ON fp_ds OFF "
		},
	"device":
		{
		"product": "FILE-RAW",
		"vendor": "",
		"serial": "",
		"setting": "rate 1536K file posterholt.raw format CU8"
		},
	"msgs": [ 
		{"class":"AIS","device":"AIS-catcher","rxtime":"20221102171324","scaled":true,"channel":"A","nmea":["!AIVDM,1,1,,A,13`fL1PP140KCELMBO7SS?wH0@Jv,0*50"],"ppm":0.000000,"type":1,"mmsi":244030470,"status":0,"status_text":"Under way using engine","speed":6.800000,"accuracy":false,"lon":5.964237,"lat":51.185970,"course":90.800003,"repeat":0,"second":44,"maneuver":0,"raim":false,"radio":67262}
	]
}
```
We can use this functionality to submit data to [APRS.fi](https://aprs.fi) directly without the need of middleware:
```console
AIS-catcher -H http://aprs.fi/jsonais/post/secret-key ID callsign PROTOCOL aprs INTERVAL 30 -q
```
Where ``secret-key`` should be your password and ``callsign`` your callsign.  The ``PROTOCOL`` setting instructs AIS-catcher to submit JSON in a form that is accepted by APRS.fi which is a multi-form HTTP message. The response from the server will be printed on screen, if you want to show this message only in case of an error, add `RESPONSE OFF` to the argument.

Another example of this HTTP feed functionality is to provide data to [Chaos Consulting](https://adsb.chaos-consulting.de/map/) without the need to install any additional scripts. The Chaos Consulting server has been set up so that it can read the AIS-catcher JSON format as per above:
```console
AIS-catcher -H https://ais.chaos-consulting.de/shipin/index.php USERPWD Station:Password GZIP on INTERVAL 5
```
Notice that this server requires authentication with a station name and password and accepts JSON with gzip encoding which significantly reduces bandwidth. 

**Important**: to use and build AIS-catcher with HTTP support, please install the following libraries before running cmake:
```console
sudo apt install libssl-dev zlib1g-dev
```
This step is only required if you want to ZIP content and post data to secure servers.

The supported protocol switches are ``AISCATCHER`` (default), ``MINIMAL`` (NMEA lines and metadata), ``LINES`` (one JSON message per line), ``APRS`` (to submit to APRS.fi).

### Forwarding AIS messages over UDP and TCP

AIS messages can be forwarded between applications over UDP via the `-u` switch and TCP using `-P` as we have seen in the examples above.
Additionally, AIS-catcher has the option to send NMEA messages packaged in a JSON object:
```console
AIS-catcher -u 192.168.1.235 4002 JSON on
```
This will send over the NMEA lines plus additional metadata like signal level etc. AIS-catcher can also listen and process NMEA and JSON input when running as a UDP server, e.g.:
```console
AIS-catcher -x 192.168.1.235 4002
```
Most external programs will not be able to accept these JSON-packaged NMEA strings. It is a way to transfer received messages between AIS-catcher instances without losing metadata like the timestamp, ppm correction and signal level. These are not captured in the standard NMEA strings. 

Another option for UDP sending via `-u` is `BROADCAST on/off` to enable sending to broadcast addresses.

A feature has been added that sends messages to (e.g.) MarineTraffic as a TCP client (with auto-reconnect) using the `-P` switch. For example:
In this case, AIS-catcher acts as a TCP client and connects to the remote listener at 192.168.1.239 port 2947. You can also set up AIS-catcher as a TCP listener itself for sending NMEA messages, i.e. the program acts as a TCP server where at most 64 clients can connect to and read NMEA lines:
```console
AIS-catcher -S 5011
```
AIS-catcher itself can also read from such a TCP NMEA server:
```console
AIS-catcher -t txt 192.168.1.239 5011
```
The `txt` indicator is the connection protocol to distinguish it from reading raw IQ samples from an RTL-TCP connection, see below.

#### Connecting to OpenCPN
In this example, we have AIS-catcher running on a Raspberry PI and aim to receive the messages in OpenCPN running on a Windows computer with IP address ``192.168.1.239``. We have chosen to use port ``10101``. On the Raspberry, we start AIS-catcher with the following command to send the NMEA messages to the Windows machine:
```console
 AIS-catcher -u 192.168.1.239 10101
 ```
 
In OpenCPN the only thing we need to do is create a Connection with the following settings:

<p align="center">
<img src="https://raw.githubusercontent.com/jvde-github/AIS-catcher/eb6ac606933f1793ad04f56fa58c92ae49171f0c/media/OpenCPN%20settings.jpg" width=40% height=40%>
</p>

### Filtering Messages by Type

AIS-catcher has functionality to filter UDP, HTTP and screen output on message type, e.g. send only messages of type 1, 2, 3, 5, 18, 19, 24 and 27 over UDP:
```console
AIS-catcher -u 127.0.0.1 10110 FILTER on ALLOW_TYPE 1,2,3,5,18,19,24,27
```
or remove message type 6 and 8:
```console
AIS-catcher -u 127.0.0.1 10110 FILTER on BLOCK_TYPE 6,8
```
Do not use spaces in the comma-separated message type list as it confuses the command line. Filtering will only take effect with the filter switched to ``ON`` (default ``OFF``) and the filter needs to be defined per ``-u`` switch (or ``-H`` and ``-o``).

In my home station, I am using this to control the size of the log file but still capture messages for inspection later. I am running with the command line parameter:
```console
AIS-catcher -o 5 filter on block_type 1,2,3,4,5,9,18,19,21,24
```
Message type 8 is region-specific. If you encounter any messages in the wild that might be interesting for AIS-catcher to parse, please share in the Issue section and we can see if it is worthwhile to extend the JSON generator. 

**Note**: filtering for messages to screen can only be set on the command line and not in the JSON configuration file at this stage. UDP filtering is available in the JSON configuration file.

### Input from file and stdin

AIS-catcher can read from a file with the switch ``-r`` followed by the filename and with a ``.`` or ``stdin`` it reads from stdin, e.g. ``-r .``. The following command records a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and pipes it to AIS-catcher for decoding:
```console
rtl_sdr -s 288K -f 162M  - | AIS-catcher -r . -s 288K -v
```
The same mechanism can be used to apply other transformations on the signal, e.g. downsampling with ``sox``:
```console
sox -c 2 -r 1536000 -b 8 -e unsigned -t raw posterholt.raw -t raw -b 16 -e signed -r 96000 - |AIS-catcher -s 96K -r CS16 . -v
```
For reference, as per version 0.36, AIS-catcher has the option to use the internal sox library directly if included in your build:
```console
AIS-catcher -s 1536K -r CU8 posterholt.raw -v -go SOXR on 
```
Default assumption is that the file is in raw unsigned 8-bit IQ format. Alternative formats can be set via `-gr` (see below) and can even include NMEA strings in text input. 


### Long Range AIS messages and alternative channels

AIS-catcher has the option to listen at frequency 156.8 Mhz to receive Channel 3/C and 4/D (vs the default A and B around 162 MHz) with the switch ```-c CD```. This follows ideas from a post on the [Shipplotter forum](https://groups.io/g/shipplotter/topic/ais_type_27_long_range/92150532?p=,,,20,0,0,0::recentpostdate/sticky,,,20,2,0,92150532,previd%3D1657138240979957244,nextid%3D1644163712453715490&previd=1657138240979957244&nextid=1644163712453715490). The default decoder is available with the switch ```-c AB```. Note that ``gpsdecode`` cannot handle channel designations C and D in NMEA lines. You can provide an optional argument to use channel designations A and B in the NMEA line with the command ```-c CD AB```.

In a similar fashion `-c X` will decode one channel. This is only useful in some instances, see the ZMQ example below.

### NMEA2000 input and output via SocketCAN

In v0.56 we introduced "Experimenter Mode" for NMEA2000 input and output via socketCAN on Linux. To properly handle the mechanics of a NMEA2000 network, the NMEA2000 library by  Timo Lappalainen
is required, build AIS-catcher in the main directory with:
  ```
  ./scripts/build-NMEA2000.sh
  ```
  This downloads and builds the [NMEA2000 library](https://github.com/ttlappalainen/NMEA2000) and includes it in the AIS-catcher build process.
  The following example creates a UDP server listening on port 4002 and forwards these messages to the CAN-bus (`vcan0`):
  ```console
  AIS-catcher -x 192.168.1.120 4002 -I vcan0  
  ```
  Current implementation handles AIS messages 1-5, 9, 11, 14, 18, 19, 21, 24 and have been very high-level tested with the excellent [CANboat](https://github.com/canboat/canboat) utilities and a virtual network.
  Another option is to have AIS-catcher read AIS messages on the NMEA2000 canbus:
  ```console
  AIS-catcher -i vcan0
  ``` 
  Note that this only works on Linux with socketCAN support and has not been tested properly. Obviously, the program is not certified by NMEA and is not build for connecting it to a NMEA2000 network on a boat. It is for the experimenters wanting to learn and play with networks and AIS.
  
### Configuration file

As per version 0.41, AIS-catcher can be mostly configured via a configuration file in JSON format,
```console
AIS-catcher -C config.json
```
where `config.json` is the name of the configuration file. The idea behind this feature is to simplify the setup of feeding multiple online sources. The minimal configuration file should have the following:
```json
{ "config": "aiscatcher", "version": 1 }
```
A fuller example config file looks as follows:
```json
{
    "config": "aiscatcher",
    "version": "1",
    "input": "serialport",
    "verbose": true,
    "sharing": true,
    "sharing_key": "6ef40ea8-59b9-11ef-8db4",
    "screen": 0,
    "serialport": {
        "baudrate": 38400,
        "port": "/dev/tty0"
    },
    "rtlsdr": {
        "active": true,
        "rtlagc": true,
        "tuner": "auto",
        "bandwidth": "192K",
        "sample_rate": "1536K",
        "biastee": false,
        "buffer_count": 2
    },
    "airspy": {
        "sample_rate": "3000K",
        "linearity": 17,
        "biastee": false
    },
    "airspyhf": {
        "sample_rate": "192k",
        "threshold": "low",
        "preamp": false
    },
    "hackrf": {
        "sample_rate": "6144k",
        "lna": 8,
        "vga": 20,
        "preamp": false
    },
    "sdrplay": {
        "sample_rate": "2304K",
        "agc": true,
        "lnastate": 5,
        "grdb": 40
    },
    "udpserver": {
        "server": "192.168.1.235",
        "port": 4002
    },
    "server": {
        "file": "stat.bin",
        "backup": 10,
        "realtime": true,
        "active": true,
        "port": 8100,
        "station": "My Station",
        "station_link": "http://example.com/",
        "share_loc": true,
        "lat": 52,
        "lon": 4.3,
        "plugin_dir": "/home/jasper/AIS-catcher/plugins/",
        "cdn": "/home/jasper/webassets",
        "prome": true,
        "context": "settings"
    },
    "tcp": [
        {
            "active": true,
            "host": "5.9.207.224",
            "port": 12,
            "keep_alive": false
        }
    ],
    "udp": [
        {
            "host": "ais.fleetmon.com",
            "port": 0
        },
        {
            "active": true,
            "host": "hub.shipxplorer.com",
            "port": 0,
            "filter": false,
            "allow_type": "1,2,3,5,18,19,24"
        }
    ],
    "tcp_listener": [
        {
            "port": 5012
        }
    ],
    "http": [
        {
            "url": "https://ais.chaos-consulting.de/shipin/index.php",
            "userpwd": "user:pwd",
            "interval": 30,
            "gzip": false,
            "response": false,
            "filter": false
        },
        {
            "url": "http://aprs.fi/jsonais/post/secret_key",
            "id": "myid",
            "interval": 60,
            "protocol": "aprs",
            "response": false
        }
    ]
}
```

The UDP and HTTP outward connections are included as a JSON array (surrounded by `[` and `]`) with an  "object" for each separate channel. In each object we can include the 
boolean field ``active`` (see the second UDP definition) which will cause the program to ignore the settings if set to `false` providing an easy way to switch particular channels or dongle configurations on and off. 

The fields and values in the configuration file can be specified consistent with the command line settings as described 
in this document. JSON is however case sensitive so field names must be entered in lower case.

The active device is selected via the ``input`` or ``serial`` field. Sections for specific SDRs like `rtlsdr` specify the settings of the device only and **do not** automatically select it. Therefore, we can specify settings for many devices even if not connected. This will not have an impact.

If both `input` and `serial` are included in the configuration file to select a device for decoding, the program will check that they are consistent, i.e. the hardware with the specified serial number must be of type ``input``. 
If you want to run multiple receivers simultaneously, this is possible as well but the device-specific settings and selection need to be included in an array ``receiver``:
```json
{
  "config":"aiscatcher",
  "version":1,
  "receiver":[
    {
      "input":"airspy",
      "verbose":true,
      "airspy":{
        "sample_rate":"3000K"
      }
    },
    {
      "input":"rtlsdr",
      "serial":"ais",
      "verbose":true,
      "rtlsdr":{
        "bandwidth":"192k"
      }
    }
  ]
}
```
If there is only one RTL-SDR connected, only `input` set to `rtlsdr` is sufficient. Similarly, if there is only one device connected with serial `ais`, we only have to specify `serial`. 

### NMEA input

AIS-catcher can be used as a command line utility that decodes NMEA lines in a file and prints the results as JSON. It provides a way to move the JSON analysis to the server side (send over NMEA with minimal metadata) or for unit testing the JSON decoder which was the prime reason for the addition of this feature. As an example:
```console
echo '!AIVDM,1,1,,B,3776k`5000a3SLPEKnDQQWpH0000,0*78'  | AIS-catcher -r txt . -o 5
```
which produces
```json
{"class":"AIS","device":"AIS-catcher","scaled":true,"channel":"B","nmea":["!AIVDM,1,1,,B,3776k`5000a3SLPEKnDQQWpH0000,0*78"],"type":3,"repeat":0,"mmsi":477213600,"status":5,"status_text":"Moored","turn":0,"speed":0.000000,"accuracy":true,"lon":126.605469,"lat":37.460617,"course":39.000000,"heading":252,"second":12,"maneuver":0,"raim":false,"radio":0}
```
When piping NMEA text lines into AIS-catcher, use format ``TXT`` which ensures that the program immediately processes the incoming characters and will not buffer them first. The NMEA decoder can be activated with the switch `-m 5` but setting the input format to TXT will automatically activate this decoder. 

This functionality opens a few doors. For example, you can use AIS-catcher to read and forward messages from a dAISy Hat (simply read from the file ``cat /dev/serial0`` on Linux) or process the data from Norwegian coastal traffic offered via a TCP server, like this:  
```console
netcat  153.44.253.27  5631 | AIS-catcher -r txt . -o 5
```

For input via TCP, you can skip the `netcat` command and directly read the input into the program as follows:
```console
AIS-catcher -t txt 153.44.253.27 5631
```
Again, the `FORMAT txt` option switches off the buffering and automatically selects the NMEA decoder.

Finally, you can also receive NMEA input via a built-in UDP server:
```console
AIS-catcher -x 192.168.1.235 4002
```

The functionality to read NMEA lines from text files has been used to validate AIS-catcher JSON output on a [file](https://www.aishub.net/ais-dispatcher) with 80K+ lines against [pyais](https://pypi.org/project/pyais/) and [gpsdecode](https://gpsd.io/gpsdecode.html). Only available switches for this decoder are ``-go NMEA_REFRESH`` and ``-go CRC_CHECK`` which force AIS-catcher to, respectively, recalculate the NMEA lines if ``on`` (default ``off``) and ignore messages with incorrect CRC if ``on`` (default ``off``). Example: 
```console
echo '$AIVDM,1,1,,,3776k`5000a3SLPEKnDQQWpH0000,0*79' | AIS-catcher -r txt . -n -go nmea_refresh on crc_check off
```
returns a warning on the incorrect CRC and:
```
!AIVDM,1,1,,,3776k`5000a3SLPEKnDQQWpH0000,0*3A
```
Note that CRC/checksum is the simple xor-checksum for validating that the NMEA line is not corrupted and not the CRC that is transmitted with the AIS message for a decoder to check the correct reception over air. This latter 16-bit checksum/CRC is not included in the NMEA message.

AIS-catcher will also accept AIVDO input which is typically used for the MMSI of the own ship. You can enable/disable this with: `-go VDO on/off`.

### Specifying Station Location

As discussed above, the webserver will only share a known location of the station with the front-end web viewer if `share_loc` is set for the webserver:
```console
AIS-catcher -N 8100 share_loc on
```
This option is switched off by default for privacy reasons in case the web viewer is shared externally.
The NMEA decoder accepts NMEA lines from a GPS device (NMEA lines GPRMC, GPGLL and GPGGA):
```console
echo '$GPGGA, 161229.487, 3723.2475, N, 12158.3416, W, 1, 07, 1.0, 9.0, M, , , , 0000*18' | ./AIS-catcher -r txt .
```
These GPS coordinates will be used to set the location of the station. In this way, the station can be visualized and tracked while on the move. This is useful if you use AIS-catcher to read from a hardware AIS receiver that has a built-in GPS.
Another approach is to read from a GPSD server, e.g. when GPSD listens on post 2947 of the local PC: 
```console
AIS-catcher -t gpsd localhost 2947 -N 8100 share_loc on` 
```
or from a serial device:
```console
AIS-catcher -e 38400 /dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox
````
The web viewer has the options `-N use_gps on/off` and `-N own_mmsi xxxxx`. The first enables/disables the use of GPS NMEA input as the location for the receiver station (default is on). The latter sets the station's location as the location of the vessel with the specified MMSI. The own MMSI will be highlighted on the web viewer map.

### Multiple device input

The latest version can run with multiple receivers in parallel. For example, one dongle for channel A+B and one dongle for channel C+D. To run with two receivers in parallel you can use a command like:
```console
AIS-catcher -d serial1 -v -d serial2 -c CD -v -N 8100
```

These functions allow AIS-catcher to receive input from an AIS receiver over UDP and a connected GPS device in parallel, e.g.:
```console
AIS-catcher -e 38400 /dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_7_-_GPS_GNSS_Receiver-if00 -x 192.168.1.235 4002 -N 8100 share_loc on
```
The first receiver (`-e ...`) reads from a GPS device that is connected and emits NMEA lines. The second receiver (`-x`) reads AIS NMEA lines at port 4002 coming from another instance of AIS-catcher. The station is now plotted on the map with the location as provided
by the GPS coordinates. The web page has an option to fix the center of the map on the location of the receiving station (right-click on the station icon on the map).

### Writing AIS messages to a Postgres Database

As per full release `v0.45`, there is functionality to write messages to a database (PostgreSQL). The setup is fairly flexible and can be tailored to the particular needs. First create an empty PostgreSQL database, e.g on an Ubuntu distribution (this might be different on your system):
```console
sudo -u postgres createdb ais
```
Set up the necessary tables from the AIS-catcher directory:
```console
psql ais <DBMS/create.sql 
```
Make sure you build the latest version of AIS-catcher with this dependency:
```console
sudo apt install libpq-dev
```
Now AIS-catcher can write the received messages to the database:
```console
AIS-catcher -D dbname=ais STATION_ID 17
```
or when more details, like username and password, are required:
```console
AIS-catcher -D postgresql://[user[:password]@][netloc][:port][/dbname]
```
The `STATION_ID` setting is optional but will populate the entries in the database with the specified ID so multiple feeders can write to one database.
There are a few settings for the new `-D` switch of which the first is the connection string that specifies the database. If you want to use a space in the string use quotation marks around the string. There are other settings that define how tables will be populated:

| Table | Description | Settings | Default |
| :--- | :--- | :--- | :--- |
| ais_vessel | last received data per MMSI | V on/off | **on**  |
| ais_message | received messages with meta data  | MSGS on/off | off  |
| ais_nmea | nmea sentences | NMEA on/off | off |
| ais_basestation | basestation messsages from type 4 | BS on/off | off |
| ais_sar_position | sar positions from type 9 | SAR on/off | off |
| ais_aton | aton messages from type 21 | ATON on/off | off |
| ais_vessel_pos | vessel position messages from type 1-3, 18, 19, 27 | VP on/off | off |
| ais_vessel_static | vessel static data from type 5, 19 | VS on/off | off |
| ais_property | specific key/value pairs with link to message  | fill with keys specified in the table ais_keys | empty |

From hereon it is fairly straightforward to pick up this data and start analysis. If the connection fails during the decoding, for whatever reason, the program will try to reconnect to the database every 2 seconds. The maximum number of failed connection attempts before the program terminates is set with `MAX_FAILS` (<1000) and is set on the command line. If `MAX_FAILS` is 1000 the program will not terminate if the connection fails.  

I hope this is sufficient to get you experimenting! Unfortunately, the options cannot yet be set from the JSON configuration file which is work in progress.

### Running on RPI Zero W and other devices with performance limitations

AIS-catcher implements a trick to speed up downsampling for RTLSDR input at 1536K samples/second by using fixed point calculations (```-F```). In essence, the downsampling is done 
in 16-bit integers performed in parallel for the I and Q channels using only 32-bit integers.

<p align="center">
<img src="https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/raspberry.jpg" width=40% height=40%>
</p>

This feature can activated with the ```-F``` switch and is only available for RTL-SDR running at a rate 1536K per second (the default). 
To give an idea of the performance improvement on a Raspberry Pi Model B Rev 2 (700 MHz), I used the following command to decode from a file on the aforementioned Raspberry Pi:

```console
AIS-catcher -r posterholt.raw -s 1536K -b -q -v
```
Resulting in 38 messages and the ```-b``` switch prints the timing used for decoding:
```
[AIS engine v0.31]	: 17312.1 ms
```
Adding the ```-F``` switch yielded the same number of messages but the timing is now:
```
[AIS engine (speed optimized) v0.31]	: 7722.32 ms
```
On an RPI Zero W this will bring down CPU load to ~40% and avoid buffer overruns.

### Connecting to GNU Radio via ZMQ

The latest code base of AIS-catcher can take streaming data via ZeroMQ (ZMQ) as input. This eases the interface with packages like GNU Radio. The steps are simple and will be demonstrated by decoding the messages in the AIS example file from [here](https://www.sdrplay.com/iq-demo-files/). AIS-catcher cannot directly decode this file as the file contains only one channel, the frequency is shifted away from the center at 162Mhz and the sample rate of 62.5 KHz is not supported in our program. We can however perform decoding with some help from [``GNU Radio``](https://www.gnuradio.org/). First start AIS-catcher to receive a stream (data format is complex float and sample rate is 96K) at a defined ZMQ endpoint:
```console
AIS-catcher -z CF32 tcp://127.0.0.1:5555 -s 96000
```
Next we can build a simple GRC model that performs all the necessary steps and has a ZMQ Pub Sink with the chosen endpoint:
![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/SDRuno%20GRC.png)
Running this model, will allow us to successfully decode the messages in the file:

![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/SDRuno%20example.png)

The ZMQ interface is useful if a datastream from an SDR needs to be shared and processed by multiple decoders or for experimentation with different decoder models with support from GNU Radio.

Note that with [CSDR](https://github.com/ha7ilm/csdr) and [SoX](https://sox.sourceforge.net/) we can also decode this file as follows:
```console
sox SDRuno_20200907_184926Z_161985kHz.wav -t raw -b 32 -e floating-point - |csdr shift_math_cc 0.165 | AIS-catcher  -r cf32 . -s 62500 -c X -v
```

### Multiple receiver models

The command line provides the `-m```` option which selects the specific decoding model.  In the current version, 4 different receiver models are embedded for raw data samples:

- **Default Model** (``-m 2``): the default demodulation engine.
- **Base Model (non-coherent)** (``-m 1``): using FM discriminator model, similar to RTL-AIS (and GNUAIS/Aisdecoder) with tweaks to the Phase Locked Loop and main receiver filter (computed with a stochastic search algorithm).
- **Standard Model (non-coherent)** (``-m 0``): as the Base Model with brute force timing recovery.
- **FM Discriminator model**: (``-m `3`) as the Standard Model but with the input already assumed to be the output of an FM discriminator. Hence no FM demodulation takes place which allows ```AIS-catcher``` to be used as GNUAIS and AISdecoder.

The Default Model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station, it improves message count by a factor 2 - 3. The reception quality of the Standard Model over the Base Model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the Default Model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```-m 1```, if needed.

Notice that you can execute multiple models in one run for benchmarking purposes but only the messages from the first model specified are displayed on screen. To benchmark different models specify ```-b``` for timing and/or ```-v``` to compare message count, e.g.
```console
AIS-catcher -s 1536K -r posterholt.raw -m 2 -m 0 -m 1 -q -b -v
```
The program will run and summarize the performance (count and timing) of three decoding models (on a Raspberry Pi 4B):
```
[AIS engine v0.35]:                      38 msgs at 6.3 msg/s
[Standard (non-coherent)]:               4 msgs at 0.7 msg/s
[Base (non-coherent)]:                   3 msgs at 0.5 msg/s
```
```
[AIS engine v0.35]:                      1036.54 ms
[Standard (non-coherent)]:               932.47 ms
[Base (non-coherent)]:                   859.065 ms
```
In this example the Default Model performs quite well in contrast to the Standard non-coherent engine with 38 messages identified versus 4 for the standard engine. 
This is typical when there are messages of poor quality. However, it increases the decoding time a bit and has a slightly higher memory usage so needs more powerful hardware. Please note that the improvements seen for this particular file are an exception.

For completeness, the decoder for NMEA input as text is activated by `-m 5` and `-m `4` is an experimental implementation to test new ideas. In practice, the user will not require these settings.


### Input from FM discriminator
We can run AIS-catcher on a RAW audio file as in this [tutorial](https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48K -r cs16 helsinki-210-messages.raw
```
On this file we should extract roughly ``362`` AIVDM lines. Notice that with switch ```-m 3``` on the command line AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case, the program is similar to the following usage of GNUAIS:
```console
gnuais -l helsinki-210-messages.raw
```
which produces:
```
INFO: A: Received correctly: 153 packets, wrong CRC: 49 packets, wrong size: 4 packets
INFO: B: Received correctly: 52 packets, wrong CRC: 65 packets, wrong size: 10 packets
```

## Device specific settings

The command line allows you to set some device-specific parameters. AIS-catcher follows the default settings and naming conventions for the devices as much as possible so that optimal parameters determined by SDR software for signal analysis (e.g. SDR#, SDR++, SDRangel) can be directly copied. Below some examples. Note that these settings are not selecting the device used for decoding itself, this needs to be done via the ``-d`` switch (or `-e/s/r/z`). If a device is not connected or used for decoding any specific settings are simply ignored.

### RTL SDR
Gain and other settings specific to the RTL SDR can be set on the command line with the ```-gr``` switch. For example, the following command sets the tuner gain to +33.3 and switches the RTL AGC on:
```console
AIS-catcher -gr tuner 33.3 rtlagc ON
```
Settings are not case-sensitive.

### AirSpy HF+
Gain settings specific for the AirSpy HF+ can be set on the command line with the ```-gh``` switch. The following command switches off the preamp:
```console
AIS-catcher -gh preamp OFF
```
Please note that only AGC mode is supported so there are limited options.

### AirSpy Mini/R2

The AirSpy Mini/R2 requires careful gain configuration as described [here](https://airspy.com/quickstart/). As outlined in that reference there are three different gain modes: linearity, sensitivity and so-called free. These can be set via the ```-gm```switch when using the AirSpy. We can activate 'linearity' mode with gain ```10```using the following ```AIS-catcher``` command line:
```console
AIS-catcher -gm linearity 10
```
Finally, gains at different stages can be set as follows:
```console
AIS-catcher -gm lna AUTO vga 12 mixer 12
```
More guidance on setting the gain model and levels can be obtained in the mentioned link.

### SDRPlay RSP1/RSP1A/RSPDX (API 3.x)
Settings specific for the SDRPlay  can be set on the command line with the ```-gs``` switch, e.g.:
```console
AIS-catcher -gs lnastate 5
```

### Serial Port
Settings specific for reading NMEA lines from a serial port can all be set with the `e` switch fow now, e.g. on Linux:
```console
AIS-catcher -e 368400 /dev/serial1
```
To dump the raw input from the serial device on-screen use `-`ge print on`.

### HackRF
Settings specific for the HackRF can be set on the command line with the ```-gf``` switch, e.g.:
```console
AIS-catcher -gf lna 16 vga 16 preamp OFF
```

### RTL TCP and SpyServer
AIS-catcher can process the data from a [`rtl_tcp`](https://projects.osmocom.org/projects/rtl-sdr/wiki/Rtl-sdr#rtl_tcp) process running remotely, e.g. if the server is on `192.168.1.235` port `1234` with a sampling rate of `240K` samples/sec:
```console
AIS-catcher -t 192.168.1.235 1234 -gt TUNER auto
```
For [SpyServer](https://airspy.com/)  use the ``-y`` switch like:
```console
AIS-catcher -y 192.168.1.235 5555 -gy GAIN 14
```
### SoapySDR

In general we recommend to use the built-in drivers for supported SDR  devices. However, AIS-catcher also supports a wide variety of other devices via the [SoapySDR library](https://github.com/pothosware/SoapySDR/wiki) which is an independent SDR support library. SoapySDR is not included by default in the standard build. To enable SoapySDR support follow the build instructions below but replace the ```cmake``` call with:
```console
cmake .. -DSOAPYSDR=ON
```
The result is that AIS-catcher adds a few additional "devices" to the device list (```-l```): a generic SoapySDR device and one device for each receiving channel for each device, e.g. with one RTL-SDR dongle connected this would look like:
```
Found 3 device(s):
0: Realtek, RTL2838UHIDIR, SN: 00000001
1: SOAPYSDR, 1 device(s), SN: SOAPYSDR
2: SOAPYSDR, driver=rtlsdr,serial=00000001, SN: SCH0-00000001
```
To start streaming via Soapy we can use:
```console
AIS-catcher -d SCH0-00000001
```
Note that the serial number has a prefix of ```SCH0``` (short for SoapySDR Channel 0) to distinguish it from the device accessed via the native SDR library. Alternatively, we can use a device-string to select the device: 
```console
AIS-catcher -d SOAPYSDR -gu device "serial=00000001,driver=rtlsdr" -s 1536K
```
Stream arguments and gain arguments can be set similarly via ```-gu STREAM``` and ```-gu GAIN``` followed by an argument string (if it contains spaces use ""). Please note that SoapySDR does not signal if the input parameters for the device are not set properly. We therefore added the ```-gu PROBE on``` switch which displays the actual settings used, e.g.
```console
AIS-catcher -d SOAPYSDR -s 1536K -gu GAIN "TUNER=37.3" PROBE on SETTINGS "biastee=true"
```
To complete the example, this command also sets the tuner gain for the RTL-SDR to 37.3 and switches on the bias-tee via the SETTING command giving access to the device's extra settings.

If the sample rates for a device are not supported by AIS-catcher, the SOXR functionality could be considered (e.g. ```-go SOXR on```). Again, we advise to use the built-in drivers and include resampling functionality where possible.  

## Validation
### Experiment at the [Meteotoren in Scheveningen](https://www.meteotoren.nl/index.php?id=index) 

On August 25, 2022 I was given the opportunity to connect AIS-catcher for a few minutes to the antenna system at the [Meteotoren](https://www.meteotoren.nl/index.php?id=ais) which has a consistently high message rate and availability on [MarineTraffic](https://www.marinetraffic.com/en/ais/details/stations/15981). 

We ran AIS-catcher on a laptop for 60 seconds and counted the number of messages for two RTL-SDR dongles (```-gr rtlagc on -T 60 -v 60```): 

| SDR | Run 1 | Run 2 |
| :--- | :--- | :---: |
| RTL-SDR blog v3 | 1061 | 1255 |
| ShipXplorer AIS dongle |  1372 | 1315 |

The ShipXplorer AIS dongle, as far as I can see, is an RTL-SDR with an additional SAW filter (TA0395A). The two sets of runs suggest some advantages of using a dongle with a filter. For reference, the AIS-catcher default decoder showed roughly a 30% improvement over an FM-based decoder in message count. An important factor of the high message rate at the Meteotoren though seems to stem from the location and the installed Yagi antenna. An experiment where we reran with a standard antenna placed at a slightly lower height reduced the message count to below 800 messages per second. 

<p align="center">
<img src="https://user-images.githubusercontent.com/52420030/220280154-602637b2-5874-455f-86fd-cd55e6d51573.png" width=30% height=30%>
<img src="https://user-images.githubusercontent.com/52420030/220280223-264e05da-9ccd-45b1-a5a7-58188e54f8de.png" width=30% height=30%>
</p>

Meteotoren feeds MarineTraffic with a [Comar SLR350NI](https://help.marinetraffic.com/hc/en-us/articles/227724587-Comar-SLR-350Ni). According to the MarineTraffic statistics the message count just prior and just after the experiment was in the area of 1350 messages/minute. We did not observe a difference in range with the MarineTraffic statistics to conclude (see pictures - left is AIS-catcher reception for a few minutes visualized with AISdispatcher, right is a screenshot from MarineTraffic).
These initial results are promising and it would be interesting to compare, in a more scientific manner, how open-source decoders with a generic RTL-SDR and dedicated AIS receiver hardware compare. Thank you Meteotoren for facilitating!

### Experimenting with recorded signals
The functionality to receive radio input from `rtl_tcp` provides a route to compare different receiver packages on a deterministic input from a file. I have tweaked the callback function in `rtl_tcp` so that it instead sends over input from a file to an AIS receiver like `AIS-catcher` and `AISrec`. The same trick can be easily done for `rtl-ais`. The sampling rate of the input file was converted using `sox` to 240K samples/second for `rtl-tcp` and 1.6M samples/second for `rtl-ais`. 
These programs, and others like `gnuais` have been the pioneers in the field of open-source AIS decoding and without them many related programs including this one would arguably not exist.
The output of the various receivers was sent via UDP to AISdispatcher which removes any duplicates and counts messages. The results in terms of number of messages/distinct vessels:
 | File | AIS-catcher v0.35  | AIS-catcher v0.33 | rtl-ais | AISrec 2.208 (trial - super fast) | AISrec 2.208 (pro - slow2)  | AISrec 2.301 (pro - slow2) | Source |
 | :--- | :--- | :---: | :---: | :---: | :---: | :---: |  :---: | 
 |Scheveningen |   44/37| 43/37  | 17/16 | 30/27 | 37/31 | 39/33 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Moscow| 213/35 |210/32 | 146/27 |  195/31 |  183/34 | 198/35 | shared by user @ 1920K in [discussion](https://github.com/jvde-github/AIS-catcher/issues/7) |
 |Vlieland | 93/54 |93/53  | 51/31| 72/44 | 80/52 | 82/50 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Posterholt |  39/22  |39/22 |2/2 | 13/12 | 31/21 | 31/20 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 
 **Update 1:** AISrec had a version update of 2.208 (October 23, 2021) with improved stability and reception quality and the table above has been updated to include the results from this recent version. 

 **Update 2:** Feverlaysoft has kindly provided me with a license for version 2.208 of AISrec allowing access to additional decoding models. Some experimentation suggests that "Slow2" works best for these particular examples and has been included in the above overview.
 
 **Update 3:** AISrec had a version update to 2.301 (April 17, 2022) with reduced runtime and the table above has been updated to include the results from this recent version. 
 
### Some stations with AIS-catcher

A list of some stations mentioning using AIS-catcher:
- [A caruna, Spain](https://www.marinetraffic.com/en/ais/details/stations/23439)
- [Asendorf, Germany](https://www.marinetraffic.com/en/ais/details/stations/19365)
- [Blackfield 01, UK](https://www.marinetraffic.com/en/ais/details/stations/22665)
- [Boston, US](https://www.marinetraffic.com/en/ais/details/stations/22977)
- [Chaos Consulting, Germany](https://adsb.chaos-consulting.de/map/)
- [Edinburgh, UK](https://www.marinetraffic.com/en/ais/details/stations/11523)
- [Haiphong, Vietnam](https://www.marinetraffic.com/en/ais/details/stations/23016)
- [La Linea de la Concepcion, Spain](https://www.marinetraffic.com/en/ais/details/stations/13854)
- [Naha, Okinawa](https://www.marinetraffic.com/en/ais/details/stations/15306)
- [Oranjeplaat Arnemuiden, NL](https://www.marinetraffic.com/en/ais/details/stations/17136)
- [Pickwick Landing, USA](https://www.marinetraffic.com/en/ais/details/stations/25896)
- [SeaRange AIS receiver](https://www.shipxplorer.com/searange-ais-receiver)
- [Seattle Capitol Hill, US](https://www.marinetraffic.com/en/ais/details/stations/14916)
- [Troguarat, France](https://www.marinetraffic.com/en/ais/details/stations/21360)
- [Tyres, Sweden](https://www.marinetraffic.com/en/ais/details/stations/22269)
- [Vancouver North, Canada](https://www.marinetraffic.com/en/ais/details/stations/29475) with hardware description [here](https://github.com/jvde-github/AIS-catcher/discussions/182).
- [Vancouver West End, Canada](https://www.marinetraffic.com/en/ais/details/stations/7029)
- [Vasa, Finland](https://www.marinetraffic.com/en/ais/details/stations/14994)
- [Vernouillet, France](https://www.marinetraffic.com/en/ais/details/stations/10668)
- [Vlissingen, NL](https://www.marinetraffic.com/nl/ais/details/stations/17133)

AIS-catcher connected to a commercial AIS receiver via serial port:
- [Wren Road Rab 2](https://www.marinetraffic.com/en/ais/details/stations/9615)
- [Baltimore, Ireland](https://www.marinetraffic.com/en/ais/details/stations/10083/_:6fee18c796a96183f56c876a2b26bac6)

## Build process

### Windows Binaries

Links to fully built Windows binaries of recent releases are provided in the below table, with and without SDRPlay support (which requires a running SDRPlay API). 
Running ``AIS-catcher`` should be a simple matter of unpacking the ZIP file in one directory and starting the executable on the command line with the required parameters or by clicking ``start.bat`` which you can edit with Notepad to set desired parameters.

It will likely run out of the box in case you have already RTL-SDR software running on your PC. In case you encounter an issue or crash, you might want to check:
- installation of RTL-SDR drivers is done via [Zadig](https://www.rtl-sdr.com/tag/zadig/)
- installation of the Visual Studio runtime [libraries](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).

The AIS-catcher executables are build with the latest Windows MSVC compiler. Please update your [library]([libraries](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)) before starting the below executables. Issues have been reported on Windows 10.

Recent releases:
 | Version | Win32  | x64 |  Win32 + SDRPlay | x64 + SDRPlay | 
 | :--- | :--- | :---: |   :--- | :---: | 
|Edge| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/Edge/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/Edge/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/Edge/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/Edge/AIS-catcher.SDRPLAY.x64.zip) |
|v0.60| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.60/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.60/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.60/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.60/AIS-catcher.SDRPLAY.x64.zip) |
|v0.59| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.59/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.59/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.59/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.59/AIS-catcher.SDRPLAY.x64.zip) |
|v0.58| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.58/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.58/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.58/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.58/AIS-catcher.SDRPLAY.x64.zip) |

 
If you are looking for a Windows version for the latest development version, it is automatically produced by the standard workflow and referenced in the table above.

### Ubuntu, Raspberry Pi, macOS, MSVC
The steps to compile AIS-catcher for RTL-SDR dongles are fairly straightforward on most systems. There are various options including a standard Makefile, a ```solution``` file for MSVC (see next section) and you can use ```cmake```, as we will detail now.

The first step is to ensure you have the necessary dependencies and build tools installed for your device(s). 
For example, the following installs the minimum build tools for Ubuntu and Raspberry Pi:
```console
sudo apt-get update
sudo apt-get upgrade

sudo apt-get install git make gcc g++ cmake pkg-config -y
```
For MACos with `brew` installed:
```console
brew update
brew upgrade

brew install git make gcc cmake pkg-config
```

AIS-catcher requires libraries for the particular hardware you want to use. The following table summarizes the installation instructions for all supported hardware:

  System              | Linux/Raspberry              | macOS  |  MSVC/vcpkg   |     MSVC/PothosSDR |
 :--            | :--			| :--				| :--  | :--: | 
 Command  | *sudo apt install ...*      | *brew install ...* | *vcpkg install ...*            | [*Download*](https://downloads.myriadrf.org/builds/PothosSDR/) |
***RTL-SDR***          | librtlsdr-dev          | librtlsdr  | rtlsdr rtlsdr:x64-windows             | included |
***Airspy***          | libairspy-dev                             | airspy |    -                    | included |
***Airspy HF+***        | libairspyhf-dev                            | airspyhf  |    -                | included |
***HackRF***          | libhackrf-dev                             | hackrf    |    -                 | included |
***SDRplay 1A***         | [API 3.x](https://www.sdrplay.com/downloads/) | - | [API 3.x](https://www.sdrplay.com/downloads/)     | [API 3.x](https://www.sdrplay.com/downloads/)  |
***SoapySDR***             | libsoapysdr-dev     |       |                  | X |
***ZeroMQ***             | libzmq3-dev     | zeromq      | ZeroMQ ZeroMQ:x64-windows                  | included |
***HTTP secure***             | libssl-dev  | | openssl openssl:x64-windows | X |
***ZIP***             | zlib1g-dev | | zlib zlib:x64-windows | X |


Once the dependencies are in place, the process of installing AIS-catcher  on Linux-based systems becomes:
```console
git clone https://github.com/jvde-github/AIS-catcher.git --depth 1
cd AIS-catcher
mkdir build
cd build
cmake ..
make
sudo make install
```
For the SDRPlay the software needs to be downloaded and installed from the website of the manufacturer. Once installed, the AIS-catcher build process automatically includes it in the build if available. 


### Running as a service on Ubuntu and ArchLinux

Github user abcd567a has developed a nice [script](https://github.com/abcd567a/install-aiscatcher) and manual to automatically build AIS-catcher and set it up as a background service. I tested it on Ubuntu and advice to first systematically identify the optimal settings as described above starting with ``-s 1536K -gr tuner auto rtlagc on -a 192K``. It is paramount that the settings are edited:
```
sudo nano /usr/share/aiscatcher/aiscatcher.conf 

```
For ArchLinux consult the following [link](https://github.com/abcd567a/archlinux-aiscatcher) from the same author.

### New Android version
The Android version of AIS-catcher has been recently overhauled and can be found [here](https://github.com/jvde-github/AIS-catcher-for-Android).

<p align="center">
<img width="40%" alt="image" src="https://github.com/jvde-github/AIS-catcher/assets/52420030/59176a10-dc46-4ff8-9d43-abb2baa0f7c5">
</p>


### Microsoft Visual Studio 2019+ via solution file (RTL-SDR/ZMQ only)

Ensure that you have ```vcpkg``` [installed](https://vcpkg.io/en/getting-started.html) and integrated into Visual Studio via ```vcpkg integrate install``` (as Administrator). Then install the rtl-sdr drivers as follows:
```
vcpkg install rtlsdr rtlsdr:x64-windows ZeroMQ ZeroMQ:x64-windows soxr soxr:x64-windows
```
The included solution file in the mscv directory allows you to build AIS-catcher with RTL-SDR/ZMQ support in the Visual Studio IDE.

## Container images

Pre-built container images containing AIS-catcher are available from the GitHub Container Registry. Available container tags are documented on the [package's page](https://github.com/jvde-github/AIS-catcher/pkgs/container/ais-catcher), with `latest` (the latest release) and `edge` (the bleeding edge of the `main` branch) being the two main ones.

The following `docker run` command provides an example of the usage of this container image, running the latest release of AIS-catcher interactively:

```console
docker run --rm -it --pull always --device /dev/bus/usb ghcr.io/jvde-github/ais-catcher:latest <ais-catcher command line options>
```

Alternatively, the following `docker-compose.yml` configuration provides a good starting point should you wish to use [Docker Compose](https://docs.docker.com/compose/):

```yaml
services:
  ais-catcher:
    command: <ais-catcher command line options> (e.g. -N 8100)
    container_name: ais-catcher
    ports:
      - 8100:8100 <don't forget to passthrough ports for the webclient>
    devices:
      - "/dev/bus/usb:/dev/bus/usb"
    image: ghcr.io/jvde-github/ais-catcher:latest
    restart: always
```
Please note that the SDRplay devices are currently not supported in the Docker images.

### More Docker options
To pull the latest docker image (e.g. to create or refresh to the latest version) without running:
```console
docker pull ghcr.io/jvde-github/ais-catcher:edge
```
To start AIS-catcher, you can then use:
```console
docker run --device /dev/bus/usb --rm -it ghcr.io/jvde-github/ais-catcher:edge
```
Notice that if you want to run the webviewer (-N 8100) you need to make that available on the host system with (`-p 8100:8100`). To send UDP data to OpenCPN running on the host computer, you can try to find the bridge network address (`sudo docker network inspect bridge` as per [tutorials](https://www.geeksforgeeks.org/how-to-use-docker-default-bridge-networking/) and use this as UDP destination address (e.g. `-u 172.17.0.1  5077`). Alternatively you could use `--network host` although less desirable. Please consult the Docker documentation.

An excellent Docker set-up is the [docker-shipfeeder](https://github.com/sdr-enthusiasts/docker-shipfeeder) that provides a user friendly way to 
feed various aggregators with excellent documentation and user support by the sdrenthusiasts community.

## Considerations

### A note on device sample rates
AIS-catcher automatically sets an appropriate sample rate depending on your device but provides the option to overwrite this default using the ```-s``` switch. For performance reasons, you can decide to use a lower rate or improve the sensitivity by picking a higher rate than the default. The decoding model supports most sample rates above 96K but will internally upsample a signal, if needed, to one of the following rates:
```
96K, 192K, 288K, 384K, 768K, 1536K, 3072K, 6144K, 12288K 
```
There is no efficiency advantage of using other rates than in this list apart from limiting the bandwidth and data throughput. Ideally, consider using an option from the list as it avoids upsampling (and additional noise) but it is not required and the model works well with other sampling rates.

In recent versions of AIS-catcher you can use the ``SOXR`` or ``libsamplerate`` (SRC) library for downsampling. In fact, you can compare the four different downsampling approaches with a command like:
```console
AIS-catcher -r posterholt.raw -m 2 -m 2 -go FP_DS on  -m 2 -go SOXR on -m 2 -go SRC on -b -q -v
```
which produces:
```
[AIS engine v0.35 ]:                     41 msgs at 4.1 msg/s
[AIS engine v0.35 FP-DS ]:               41 msgs at 4.1 msg/s
[AIS engine v0.35 SOXR ]:                41 msgs at 4.1 msg/s
[AIS engine v0.35 SRC]:                  41 msgs at 4.1 msg/s
```
with the following timings:
```
[AIS engine v0.35 ]:                     320.624 ms
[AIS engine v0.35 FP-DS ]:               254.341 ms
[AIS engine v0.35 SOXR ]:                653.716 ms
[AIS engine v0.35 SRC]:                  3762.6 ms
```
Some libraries will require significant hardware resources. The advice is to use the native built-in downsampling functionality but it is fun to experiment.

The default downsampler uses a simple but efficient CIC5 filter. To mitigate some of the drawbacks of this method, version 0.39 onwards uses by default a simple droop compensator in the form of a fast 3 tap filter which can be switched off with the switch ``-go DROOP off``. 
The following results are from my home station running for a few hours with the various methods running in parallel and counting the number of messages:

| Downsampler | RTL-SDR @ 1536K  | AirSpy HF+ @ 192K  | SDRPlay RSPdx @ 3072K | 
| :--- | :--- | :--- | :-- | 
|``-go DROOP off``	| 94219 |16022 | 16530 |
|``-go DROOP on`` (default) | 98176 (+4.20%) | 16265 (+1.52%) | 17190 (+3.99%) |
|``-go SOXR on`` (SOX downsampling)	| 97652 (+3.64%) | 16209 (+1.17%) | 17049 (+3.14%) |

For reference, the command line instruction to test is:
```console
AIS-catcher  -v 10 -gr rtlagc on -m 2 -go droop off -m 2 -m 2 -go soxr on
```
Please note that the runs are performed on different days over different time spans so this does not represent a comparison of devices but you can compare within a column.

### Frequency Correction
AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing, we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. [see](https://github.com/steve-m/kalibrate-rtl) and provide this as input via the ```-p``` switch on the command line.

If you are using a cheap RTL-SDR dongle that suffers from thermal drift (i.e. the required PPM correction drifts when the dongle is getting warmer), you can use the option ``-go AFC_WIDE on`` (which is the default model in recent releases). This is a relatively new model (per v0.48) that is less sensitive to frequency drift. You can switch off this model using the switch `-go AFC_WIDE off'. Running the new model setting and the previous default yields results that are more stable for frequency drift.

<p align="center">
<img width="30%" alt="image" src="https://github.com/jvde-github/AIS-catcher/assets/52420030/41c86f20-5bc3-4e83-be15-59d538820a52">
<img width="30%" alt="image" src="https://github.com/jvde-github/AIS-catcher/assets/52420030/7929bfaf-6e21-485d-9a98-4e1ab5f3384d">
</p>

#### Frequency Shift and PPM
The Web Viewer include plots of what is called the `frequency shift`. The `frequency shift` is the frequency correction from the central frequencies that AIS-catcher has used to decode the signal. This value is related to  the frequency offset of the RTL-SDR dongle as discussed above but also depends on the deviations in the equipment of the sender. The number is in ppm (parts-per-million, so 1ppm ~ 162 Hz) and in some tables and in screen output the quanity is refered to as `ppm`.  Long-running averages can be used to determine the optimal ppm correction for the receiver setup. These deviations can be corrected with `-p`. Deviations between -3 and +3 will usually not impact reception quality so for modern dongles with frequency stabilization no action is required.


### System USB performance
On some laptops we observed that Windows was struggling with the high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is worthwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
```console
rtl_test -s 1536000
```
In case you observe a high number of lost data, the advice is to run AIS-catcher at a lower sampling rate for RTL SDR dongles:
```console
AIS-catcher -s 288000
```
If your system allows for it you might opt to run ```AIS-catcher``` at a sample rate of ```2304000```. 

### Known issues

- call of ```rtlsdr_close```  on Windows can result in a crash. This is a problem with the rtlsdr library and not AIS-catcher. Solution: ensure you have the latest version of the library with this patch [rtlsdr](https://github.com/osmocom/rtl-sdr/pull/18). For the shared Windows binaries I have included [this version](https://github.com/jvde-github/rtl-sdr) of the library in which I did a proper [patch](https://lists.osmocom.org/hyperkitty/list/osmocom-sdr@lists.osmocom.org/thread/WPL5MZIX7CGVDF2NECPSTZYDLACAEXRI/) to fix this issue (essentially ensuring all usb transfers have been closed before freeing memory.
- pkg-config on Raspberry Pi returns ```-L``` as library path which results in a build error. Temporarily fixed by assuming lib is in standard location, long term fix: switch to cmake
- ...

## To do

- Add Database configuration to the JSON config file
- Testing: assess the gap with commercial equipment (partially done at Meteotoren)
- Improve decoder
- Support NMEA tag blocks for timestamp
- Channel AB+CD for devices with high sample rates like the Airspy
- Offline map support
- Input of NMEA via TCP
- MTQQ support
- Feedback advanced analytics via aiscatcher.org and shipcatcher.com
- ....

