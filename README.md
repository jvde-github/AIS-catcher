# AIS-catcher - A multi-platform AIS receiver 
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL-SDR dongles (including the ShipXplorer AIS dongle), AirSpy (Mini/R2/HF+), HackRF, SDRPlay (RSP1/RSP1A/RSPDX for now), SoapySDR, input from file as well as from ZMQ and TCP servers (RTL-TCP/SpyServer). Output is send in the form of NMEA messages to either screen or broadcasted over UDP. 
The program provides the option to read and decode the raw discriminator output of a VHF receiver as well. 

![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/containership.jpg)

## Purpose

The aim of ```AIS-catcher``` is to provide a platform to facilitate continuous improvement of receiver models. Any suggestions, observation or sharing of recordings for setups where the current models are struggling is highly appreciated! 

### Disclaimer

```AIS-catcher```  is created for research and educational purposes under the GPGL v3 license. It is a hobby project and not tested and designed for reliability and correctness. 
You can play with the software but it is the user's responsibility to use it prudently. So,  DO NOT rely upon this software in any way including for navigation 
and/or safety of life or property purposes.
There are variations in the legislation concerning radio reception in the different administrations around the world. 
It is your responsibility to determine whether or not your local administration permits the reception and handling of AIS messages from ships. 
It is specifically forbidden to use this software for any illegal purpose whatsoever. 
Only use this software in regions where such use is permitted.

## Installation

Windows [Binaries](https://github.com/jvde-github/AIS-catcher/blob/main/README.md#Build-process) and Building [instructions](https://github.com/jvde-github/AIS-catcher/blob/main/README.md#Build-process) for many systems are provided below. Pre-built container images containing AIS-catcher are [available](https://github.com/jvde-github/AIS-catcher#container-images)  from the GitHub Container Registry.

## What's new?
For new features in the latest version please have a look at the [release page](https://github.com/jvde-github/AIS-catcher/releases/tag/v0.42). 
There are currently a few things under development, key one is the inclusion of a simple webserver to view the station statistics which has been first included in full release v0.42. For a live demo for an actual running station in East Boston US, see [here](https://kx1t.com/ais/). Thank you [KX1T](https://kx1t.com/) for making this available. 

Make sure you use the latest version and start the webserver as follows:
```console
AIS-catcher -N 8100
```
where ``8100`` is the port number. If you go in your browser to the IP address of the machine running AIS-catcher and specify the port (e.g. if your machine is raspberrypi, enter ``raspberrypi:8100``) you will see a menu which gives access to tabs providing insights into the reception of your station, including signal levels, ships seen, a simple map and message rate.  
<p align="center">
  <img src="https://github.com/jvde-github/AIS-catcher/blob/2df653169243a18da589c95ecb576f88cae7d521/media/Webservice%20in%20Action%20Jan%202,%202022.jpg" width="30%"/>
</p>
Current development aims at making this webinterface easily accessible on mobile devices as well (see screenshot) and enriching the experience, for example by adding the contours of a vessel where available: 
<p align="center">
  <img src="https://github.com/jvde-github/AIS-catcher/blob/fe5413ffb16cc01530b17950f982d1b8685dabd0/media/Screenshot%20ship%20shape.jpg" width="30%"/>
</p>

There is an option to provide the station name and a link to an external website which will be displayed on the Statistics page as follows:
```
AIS-catcher -N STATION Southwood STATION_LINK http://example.com
```
This could be a useful option if you want to offer the interface externally.To display the distance of received messages to your station you need to provide the coordinates as follows:
```
AIS-catcher -N LAT 50 LON 3.141592
```
All these options can be captured in the configuration file (in a section with name ``server``), see below. 

When AIS-catcher receives data that contains the dimensions of a vessel but not its heading, it will plot a circle to represent it that will enclose the ship regardless of the direction.
This commonly happens with Class B ships, if a reasonable approximation for heading, such as the course-over-ground, is available, it will be used as a proxy. Any shapes that are plotted this way will have a dashed border, to indicate that the information is incomplete. An example of this can be seen in the USS Constitution, which is shown docked in the port of Boston. 
<p align="center">
  <img src="https://github.com/jvde-github/AIS-catcher/blob/fe2e40b932c1ae456c2f8513b87386de27e255fe/media/Screenshot%20USS%20contitution.jpg" width="50%"/>
</p>

The "tag control" (above the zoom controls) will add labels to the map:
<p align="center">
  <img src="https://github.com/jvde-github/AIS-catcher/blob/4114ed895f610a13598a61a10b077aeda8565ee3/media/Screenshot%20Moored.jpg" width="50%"/>
</p>

Furthermore, the plot tab contains several plots to assess the performance of the receiver:
<p align="center">
  <img src="https://github.com/jvde-github/AIS-catcher/blob/8096b8bfa3caca6c73023ce1e708ca421292f27f/media/ScreenshotPlot.jpg" width="50%"/>
</p>
Upon restarting AIS-catcher, the history displayed in the graphs is typically lost. To preserve the state of the plots, a useful option is to save the content to a file, such as "stat.bin," at closure and to create a backup every 10 minutes. This can be accomplished with the following options:
```
AIS-catcher -N 8100 FILE stat.bin BACKUP 10
```
These are new experimental feautures so reporting of any issues encounterd is appreciated.

## Portable travel version for Android available [here](https://github.com/jvde-github/AIS-catcher-for-Android). 

If you are travelling and looking for a portable system that can be used on an Android phone or running Android on an Odroid, check out the link. 
You can download the APK from the mentioned project page or the Google Play store.

## Usage
````
use: AIS-catcher [options]

	[-a xxx - set tuner bandwidth in Hz (default: off)]
	[-b benchmark demodulation models for time - for development purposes (default: off)]
	[-c [AB/CD] - [optional: AB] select AIS channels and optionally the NMEA channel designations]
	[-C [filename] - read configuration settings from file]
	[-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]
	[-h display this message and terminate (default: false)]
	[-H [optional: url] - send messages via HTTP, for options see documentation]
	[-m xx - run specific decoding model (default: 2), see README for more details]
	[-M xxx - set additional meta data to generate: T = NMEA timestamp, D = decoder related (signal power, ppm) (default: none)]
	[-n show NMEA messages on screen without detail (-o 1)]
	[-N [optional: port][optional settings] - start http server at port, see README for details]
	[-o set output mode (0 = quiet, 1 = NMEA only, 2 = NMEA+, 3 = NMEA+ in JSON, 4 JSON Sparse, 5 JSON Full (default: 2)]
	[-p xxx - set frequency correction for device in PPM (default: zero)]
	[-q suppress NMEA messages to screen (-o 0)]
	[-s xxx - sample rate in Hz (default: based on SDR device)]
	[-T xx - auto terminate run with SDR after xxx seconds (default: off)]
	[-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]
	[-v [option: xx] - enable verbose mode, optional to provide update frequency of xx seconds (default: false)]

	Device selection:

	[-d:x - select device based on index (default: 0)]
	[-d xxxx - select device based on serial number]
	[-l list available devices and terminate (default: off)]
	[-L list supported SDR hardware and terminate (default: off)]
	[-r [optional: yy] filename - read IQ data from file or stdin (.), short for -r -ga FORMAT yy FILE filename
	[-t [host [port]] - read IQ data from remote RTL-TCP instance]
	[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]
	[-y [host [port]] - read IQ data from remote SpyServer]
	[-z [optional [format]] [optional endpoint] - read IQ data from [endpoint] in [format] via ZMQ (default: format is CU8)]

	Device specific settings:

	[-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8] ]
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


To test that the installation and/or compilation was successful (see below for instructions), a good start is the following command which lists the devices available for AIS reception:
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
This lists all devices for which support is included. If particular hardware is not listed here, you might have to install the necessary libraries and rebuild AIS-catcher.

To start AIS demodulation, print some occasional statistics (every 10 seconds) and broadcast AIS messages via UDP, we can use the following command:
```console
AIS-catcher -v 10 -u 127.0.0.1 10110 -u 127.0.0.1 10111
```
If successful, NMEA messages will start to come in, appear on the screen and send as UDP messages to `127.0.0.1` port `10110` and port `10111`. These UDP messages are the key method to use the output of AIS-catcher and visualize that in OpenCPN or directly send to AIS aggregator websites like MarineTraffic, FleetMon, VesselFinder, ShipXplorer and others. See below for more pointers on how this can be set up.
The screen messages can be suppressed with the option ```-q```. That's all there is.

For RTL-SDR devices performance can be sensitive to the device settings. In general a good starting point is the following:
```console
AIS-catcher -gr RTLAGC on TUNER auto -a 192K
```
It has been reported by several users that adding a bandwidth setting of ``-a 192K`` can be beneficial  so it is definitely worthwhile to try with and without this filter.
To find the best settings for your hardware requires some systematic experimentation whereby one parameter is changed at the time, e.g. switch RTLAGC ``on`` or ``off`` and setting the TUNER to ``auto`` and try fixed tuner gains between 0 and 50. The hardware settings available depend on the hardware and more details can be found below.

## Deep dives

### Message screen output

The output of messages to screen can be regulated with the ``-o`` switch. To suppress any messages to screen use ``-o 0`` or ``-q``. This can be useful if you run AIS-catcher as a background process. To show only simple and pure NMEA lines, use the switch ``-o 1`` or ``-n``. Example output:
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
And finally, full decoding of the AIS message is activated via ``-o 5`` (or ``-o 4`` for version 0.38 and below):
```json
{"class":"AIS","device":"AIS-catcher","rxtime":"20220729191610","scaled":true,"channel":"B","nmea":["!AIVDM,1,1,,B,33L=LN051HQj3HhRJd7q1W=`0000,0*03"],"signalpower":-44.0,"ppm":0.000000,"type":3,"repeat":0,"mmsi":230907000,"status":0,"status_text":"Under way using engine","turn":18,"speed":8.800000,"accuracy":true,"lon":24.915239,"lat":60.148106,"course":231.000000,"heading":230,"second":52,"maneuver":0,"raim":false,"radio":0}
```

Meta data is not calculated by default to keep the program as light as possible when running as a server on low spec devices but can be activated with the ```-M``` switch. The calculation of signal power (in dB) and applied frequency correction (in ppm) are activated with  ``-M D``. NMEA messages are timestamped with  ``-M T`` and additional country information from the station derived from the MMSI is included in JSON output with ``-M M``. 

There are many libraries for decoding AIS messages to JSON format. I encourage you to use your favorite library ([libais](https://github.com/schwehr/libais), [gpsdecode](https://github.com/ukyg9e5r6k7gubiekd6/gpsd/blob/master/gpsdecode.c), [pyais](https://github.com/M0r13n/pyais), etc).

### Posting messages over HTTP

Some cloud services collecting AIS data prefer messages to be periodically posted via the HTTP protocol, for example [APRS.fi](https://aprs.fi). As per version 0.29 AIS-catcher can do this directly via the ``-H`` switch. For example:
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
Where ``secret-key`` should be your password and ``callsign`` your callsign.  The ``PROTOCOL`` setting instructs AIS-catcher to submit JSON in a form that is accepted by APRS.fi and posts a multi-part message. As another example, this functionality can feed the map of [Chaos Consulting](https://adsb.chaos-consulting.de/map/) without the need to install any additional scripts. The Chaos Consulting server has been set up so that it can read the AIS-catcher JSON format as per above:
```console
AIS-catcher -H https://ais.chaos-consulting.de/shipin/index.php USERPWD "Station:Password" GZIP on INTERVAL 5
```
Notice that this server requires authentication with a station name and password and accepts JSON with gzip encoding which significantly reduces bandwidth. The supported protocol switches are ``AISCATCHER`` (default), ``MINIMAL`` (NMEA lines and meta data), ``LINES`` (one JSON message per line), ``APRS`` (to submit to APRS.fi).

As a final comment, to build AIS-catcher with HTTP support, please install the following libraries before running cmake:
```console
sudo apt install libcurl4-openssl-dev zlib1g-dev
```

### Setting up OpenCPN

In this example we have AIS-catcher running on a Raspberry PI and want to receive the messages in OpenCPN running on a Windows computer with IP address ``192.168.1.239``. We have chosen to use port ``10101``. On the Raspberry we start AIS-catcher with the following command to send the NMEA messages to the Windows machine:
```console
 AIS-catcher -u 192.168.1.239 10101
 ```
 
In OpenCPN the only thing we need to do is create a Connection with the following settings:

<p align="center">
<img src="https://raw.githubusercontent.com/jvde-github/AIS-catcher/eb6ac606933f1793ad04f56fa58c92ae49171f0c/media/OpenCPN%20settings.jpg" width=40% height=40%>
</p>

### Input from file and stdin

AIS-catcher can read from file with the switch ``-r`` followed by the filename and with a ``.`` or ``stdin`` it reads from stdin, e.g. ``-r .``. The following command records a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and pipes it to AIS-catcher for decoding:
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
### Configuration file

As per version 0.41 AIS-catcher can be partially configured via a configuration file in JSON format,
```console
AIS-catcher -C config.json
```
where `config.json` is the name of any configuration file. The idea behind this feature is to simplify the set up of feeding multiple online sources. The minimal configuration file should have the following:
```json
{ "config": "aiscatcher", "version": 1 }
```
This will provide a sanity check and helps AIS-catcher to be backward compatible with early versions of the configuration files but not the other way around (i.e., AIS-catcher v0.41 cannot read version 2 config files).

An example config file looks as follows:
```json
{
   "config":"aiscatcher",
   "version":"1",
   "serial": "00000001",
   "input": "rtlsdr",
   "rtlsdr":{
      "active":true,
      "rtlagc":true,
      "tuner":"auto",
      "bandwidth":"192K",
      "sample_rate":"1536K",
      "biastee":false,
      "buffer_count":2
   },
   "airspy":{
      "sample_rate":"3000K",
      "linearity":17,
      "biastee":false
   },
   "airspyhf":{
      "sample_rate":"192k",
      "treshold":"low",
      "preamp":false
   },
   "hackrf":{
      "sample_rate":"6144k",
      "lna":8,
      "vga":20,
      "preamp":false
   },
   "sdrplay":{
      "sample_rate":"2304K",
      "agc":true,
      "lnastate":5,
      "grdb":40
   },
   "server":{
      "file":"stat.bin",
      "backup":10,
      "active":true,
      "port":8100,
      "station":"My Station",
      "station_link":"http://example.com/",
      "lat":52.0,
      "lon":4.3
   },
   "udp":[
      {
         "host":"ais.fleetmon.com",
         "port":0
      },
      {
         "active":true,
         "host":"hub.shipxplorer.com",
         "port":0
      }
   ],
   "http":[
      {
         "url":"https://ais.chaos-consulting.de/shipin/index.php",
         "userpwd":"user:pwd",
         "interval":30,
         "gzip":false,
         "response":false
      },
      {
         "url":"http://aprs.fi/jsonais/post/secret_key",
         "id":"myid",
         "interval":60,
         "protocol":"aprs",
         "response":false
      }
   ]
}
```
The UDP and HTTP outward connections are included as a JSON array (surrounded by `[` and `]`) with an  "object" for each separate channel. In each object we can include the 
boolean field ``active`` (see the second UDP definition) which will cause the program to ignore the settings if set to `false`. 
This option  provides an easy way to switch on and off particular channels or dongle configurations. The active device is selected via the ``input`` or ``serial`` field. 
If both are included the program will check that they are consistent, i.e. the hardware with the specified serial number must be of type ``input``. 
Normally it is sufficient to include one of these fields and not both. 

The fields and values in the configuration file can be specified  consistent with the command line settings as described 
in this document. JSON is however case sensitive so field names must be entered in lower case.

### AIS-catcher as a command line NMEA decoder

AIS-catcher can be used as a command line utility that decodes NMEA lines in a file and prints the results as JSON. It provides a way to move the JSON analysis to the server side (send over NMEA with minimal meta data) or for unit testing the JSON decoder which was the prime reason for the addition of this feature. Use the model ``-m 5`` which will automatically selected if the input format is set to `TXT`, e.g.:
```console
echo '!AIVDM,1,1,,B,3776k`5000a3SLPEKnDQQWpH0000,0*78'  | AIS-catcher-r txt . -o 5
```
which produces
```json
{"class":"AIS","device":"AIS-catcher","scaled":true,"channel":"B","nmea":["!AIVDM,1,1,,B,3776k`5000a3SLPEKnDQQWpH0000,0*78"],"type":3,"repeat":0,"mmsi":477213600,"status":5,"status_text":"Moored","turn":0,"speed":0.000000,"accuracy":true,"lon":126.605469,"lat":37.460617,"course":39.000000,"heading":252,"second":12,"maneuver":0,"raim":false,"radio":0}
```
When piping NMEA text lines into AIS-catcher, use the format ``TXT`` this will also ensure that the program immediately processes the incoming characters and will not buffer them first. With this function you can use AIS-catcher to forward messages from a dAISy Hat (from file ``/cat/serial0``) or Norwegian coastal traffic, like this:  
```console
netcat  153.44.253.27  5631 | AIS-catcher -r txt . -o 5
```
This new function json decoding functionality from text files has been used to validate AIS-catcher JSON output on a [file](https://www.aishub.net/ais-dispatcher) with 80K+ lines  against [pyais](https://pypi.org/project/pyais/) and [gpsdecode](https://gpsd.io/gpsdecode.html). Only available switches for this decoder are ``-go NMEA_REFRESH`` and ``-go CRC_CHECK`` which forces AIS-catcher to, respectively, recalculate the NMEA lines if ``on`` (default ``off``) and ignore messages with incorrect CRC if ``on`` (default ``off``). Example: 
```console
echo '$AIVDM,1,1,,,3776k`5000a3SLPEKnDQQWpH0000,0*79' | ./AIS-catcher -r txt . -n -go nmea_refresh on crc_check off
```
returns a warning on the incorrect CRC and:
```
!AIVDM,1,1,,,3776k`5000a3SLPEKnDQQWpH0000,0*3A
```

### Running on hardware with performance limitations

AIS-catcher implements a trick to speed up downsampling for RTLSDR input at 1536K samples/second by using fixed point calculations (```-m 2 -go FP_DS on```). In essence the downsampling is done 
in 16 bit integers performed in parallel for the I and Q channel using only 32 bit integers.
Furthermore a new model was introduced which uses exponential moving averages in the determination of the phase instead of a standard moving average as for the default model (```-m 2 -go PS_EMA on```).

<p align="center">
<img src="https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/raspberry.jpg" width=40% height=40%>
</p>

Both features can be activated with the ```-F``` switch. 
To give an idea of the performance improvement on a Raspberry Pi Model B Rev 2 (700 MHz), I used the following command to decode from a file on the aforementioned Raspberry Pi:

```console
AIS-catcher -r posterholt.raw -s 1536K -b -q -v
```
Resulting in 38 messages and the ```-b``` switch prints the timing used for decoding:
```
[AIS engine v0.31]	: 17312.1 ms
```
Adding the ```-F``` switch yielded the same number of messages but timing is now:
```
[AIS engine (speed optimized) v0.31]	: 7722.32 ms
```
This and other performance updates make the full version of AIS-catcher run on an early version of the Raspberry Pi with reasonable processor load.

### Long Range AIS messages

AIS-catcher can be instructed to listen at frequency 156.8 Mhz to receive Channel 3/C and 4/D (vs A and B around 162 MHz) with the switch ```-c CD```. This follows ideas from a post on the [Shipplotter forum](https://groups.io/g/shipplotter/topic/ais_type_27_long_range/92150532?p=,,,20,0,0,0::recentpostdate/sticky,,,20,2,0,92150532,previd%3D1657138240979957244,nextid%3D1644163712453715490&previd=1657138240979957244&nextid=1644163712453715490) and at the request of a user. The conventional decoder is available with the switch ```-c AB``` which is also the default if nothing is specified. Note that ``gpsdecode`` cannot handle channel designations C and D in NMEA lines. You can provide an optional argument to use channel A and B in the NMEA line with the command ```-c CD AB```.

### Connecting to GNU Radio via ZMQ

The latest code base of AIS-catcher can take streaming data via ZeroMQ (ZMQ) as input. This allows for an easy interface with packages like GNU Radio. The steps are simple and will be demonstrated by decoding the messages in the AIS example file from [here](https://www.sdrplay.com/iq-demo-files/). AIS-catcher cannot directly decode this file as the file contains only one channel, the frequency is shifted away from the center at 162Mhz and the sample rate of 62.5 KHz is not supported in our program. We can however perform decoding with some help from [``GNU Radio``](https://www.gnuradio.org/). First start AIS-catcher to receive a stream (data format is complex float and sample rate is 96K) at a defined ZMQ endpoint:
```console
AIS-catcher -z CF32 tcp://127.0.0.1:5555 -s 96000
```
Next we can build a simple GRC model that performs all the necessary steps and has a ZMQ Pub Sink with the chosen endpoint:
![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/SDRuno%20GRC.png)
Running this model, will allow us to successfully decode the messages in the file:

![Image](https://raw.githubusercontent.com/jvde-github/AIS-catcher/media/media/SDRuno%20example.png)

The ZMQ interface is useful if a datastream from a SDR needs to be shared and processed by multiple decoders or for experimentation with different decoder models with support from GNU Radio.

### Multiple receiver models

The command line provides  the ```-m``` option which allows for the selection of the specific receiver models.  In the current version 4 different receiver models are embedded:

- **Default Model** (``-m 2``): the default demodulation engine.
- **Base Model (non-coherent)** (``-m 1``): using FM discriminator model, similar to RTL-AIS (and GNUAIS/Aisdecoder) with tweaks to the Phase Locked Loop  and main receiver filter (computed with a stochastic search algorithm).
- **Standard Model (non-coherent)** (``-m 0``): as the Base Model with brute force timing recovery.
- **FM Discriminator model**: (``-m 3``) as  the Standard Model but with the input already assumed to be the output of a FM discriminator. Hence no FM demodulation takes place which allows ```AIS-catcher``` to be used as GNUAIS and AISdecoder.

The Default Model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station it improves message count by a factor 2 - 3. The reception quality of the Standard Model over the Base Model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the Default Model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```-m 1```, if needed.

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


### Input from FM discriminator
We can run AIS-catcher on a RAW audio file as in this [tutorial](https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48K -r cs16 helsinki-210-messages.raw
```
On this file we should extract roughly ``362`` AIVDM lines. Notice that with switch ```-m 3``` on the command line AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case the program is similar to the following usage of GNUAIS:
```console
gnuais -l helsinki-210-messages.raw
```
which produces:
```
INFO: A: Received correctly: 153 packets, wrong CRC: 49 packets, wrong size: 4 packets
INFO: B: Received correctly: 52 packets, wrong CRC: 65 packets, wrong size: 10 packets
```

## Device specific settings

The command line allows you to set some device specific parameters. AIS-catcher follows the settings and naming conventions for the devices as much as possible so that parameters and 
settings determined by SDR software for signal analysis (e.g. SDR#, SDR++, SDRangel) can be directly copied. Below some examples. Note that these settings are not selecting the device used for decoding itself, this needs to be done via the ``-d`` switch. If a device is not connected or used for decoding any specific settings are simply ignored.

### RTL SDR
Gain and other settings specific for the RTL SDR can be set on the command line with the ```-gr``` switch. For example, the following command sets the tuner gain to +33.3 and switches the RTL AGC on:
```console
AIS-catcher -gr tuner 33.3 rtlagc ON
```
Settings are not case sensitive.

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

In general we recommend to use the built-in drivers for supported SDR  devices. However, AIS-catcher also supports a wide variety of other devices via the [SoapySDR library](https://github.com/pothosware/SoapySDR/wiki) which is an independent SDR support library. SoapySDR is not included by default in the standard build. To enable SoapySDR support follow the build instructions as per below but replace the ```cmake``` call with:
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
Note that the serial number has a prefix of ```SCH0``` (short for SoapySDR Channel 0) to distinguish it from the device accessed via the native SDR library. Alternative, we can use a device-string to select the device: 
```console
AIS-catcher -d SOAPYSDR -gu device "serial=00000001,driver=rtlsdr" -s 1536K
```
Stream arguments and gain arguments can be set similarly via ```-gu STREAM``` and ```-gu GAIN``` followed by an argument string (if it contains spaces use ""). Please note that SoapySDR does not signal if the input parameters for the device are not set properly. We therefore added the ```-gu PROBE on``` switch which displays the actual settings used, e.g.
```console
AIS-catcher -d SOAPYSDR -s 1536K -gu GAIN "TUNER=37.3" PROBE on SETTINGS "biastee=true"
```
To complete the example, this command also sets the tuner gain for the RTL-SDR to 37.3 and switches on the bias-tee via the SETTING command gives access to the device's extra settings.

If the sample rates for a device are not supported by AIS-catcher, the SOXR functionality could be considered (e.g. ```-go SOXR on```). Again, we advice to use the built-in drivers and included resampling functionality where possible.  

## Validation
### Experiment at the [Meteotoren in Scheveningen](https://www.meteotoren.nl/index.php?id=index) 

On August 25, 2022 I was given the opportunity to connect AIS-catcher for a few minutes to the antenna system at the [Meteotoren](https://www.meteotoren.nl/index.php?id=ais) which has a consistently high message rate and availability on [MarineTraffic](https://www.marinetraffic.com/en/ais/details/stations/15981). 

We ran AIS-catcher on a laptop for 60 seconds and counted the number of messages for two RTL-SDR dongles (```-gr rtlagc on -T 60 -v 60```): 

| SDR | Run 1 | Run 2 |
| :--- | :--- | :---: |
| RTL-SDR blog v3 | 1061 | 1255 |
| ShipXplorer AIS dongle |  1372 | 1315 |

The ShipXplorer AIS dongle, as far as I can see, is a RTL-SDR with an additional SAW filter (TA0395A). The two sets of runs suggest some advantage of using a dongle with a filter. For reference, the AIS-catcher default decoder showed roughly a 30% improvement over a FM-based decoder in message count. An important factor of the high message rate at the Meteotoren though seems to stem from the location and the installed Yagi antenna. An experiment where we reran with a standard antenna placed at a slightly lower height reduced the message count to below 800 messages per second. 


<p align="center">
<img src="https://github.com/jvde-github/AIS-catcher/blob/9d164c39aebea5015cb82c4debf16a459f2f43a8/media/map%20AIS%20dispatcher.png" width=30% height=30%>
<img src="https://github.com/jvde-github/AIS-catcher/blob/638535b62fd1ccf4b90fa66378b735f2e28b34f3/media/map%20MarineTraffic.png" width=30% height=30%>
</p>

Meteotoren feeds MarineTraffic with a [Comar SLR350NI](https://help.marinetraffic.com/hc/en-us/articles/227724587-Comar-SLR-350Ni). According to the MarineTraffic statistics the message count just prior and just after the experiment was in the area of 1350 messages/minute. We did not observe a difference in range with the MarineTraffic statistics to draw a conclusion (see pictures - left is AIS-catcher reception for few minutes visualized with AISdispatcher, right is a screenshot from MarineTraffic).
These initial results are promising and it would be interesting to compare, in a more scientific manner, how open source decoders with a generic RTL-SDR and dedicated AIS receiver hardware compare. Thank you Meteotoren for facilitating!

### Experimenting with recorded signals
The functionality to receive radio input from `rtl_tcp` provides a route to compare different receiver packages on a deterministic input from a file. I have tweaked the callback function in `rtl_tcp` so that it instead sends over input from a file to an AIS receiver like `AIS-catcher` and `AISrec`. The same trick can be easily done for `rtl-ais`. The sampling rate of the input file was converted using `sox` to 240K samples/second for `rtl-tcp` and 1.6M samples/second for `rtl-ais`. 
These programs, and others like `gnuais` have been the pioneers in the field of open source AIS decoding and without them many related programs including this one would arguably not exist.
The output of the various receivers was sent via UDP to AISdispatcher which removes any duplicates and counts messages. The results in terms of  number of messages/distinct vessels:
 | File | AIS-catcher v0.35  | AIS-catcher v0.33 | rtl-ais | AISrec 2.208 (trial - super fast) | AISrec 2.208 (pro - slow2)  | AISrec 2.301 (pro - slow2) | Source |
 | :--- | :--- | :---: | :---: | :---: | :---: | :---: |  :---: | 
 |Scheveningen |   44/37| 43/37  | 17/16 | 30/27 | 37/31 | 39/33 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Moscow| 213/35 |210/32 | 146/27 |  195/31 |  183/34 | 198/35 | shared by user @ 1920K in [discussion](https://github.com/jvde-github/AIS-catcher/issues/7) |
 |Vlieland | 93/54 |93/53  | 51/31| 72/44 | 80/52 | 82/50 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Posterholt |  39/22  |39/22 |2/2 | 13/12 | 31/21 | 31/20 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 
 **Update 1:** AISrec  had a version update of 2.208 (October 23, 2021) with improved stability and reception quality and the table above has been updated to include the results from this recent version. 

 **Update 2:** Feverlaysoft has kindly provided me with a license for version 2.208 of AISrec allowing access to additional decoding models. Some experimentation suggests that "Slow2" works best for these particular examples and has been included in the above overview.
 
 **Update 3:** AISrec  had a version update to 2.301 (April 17, 2022) with reduced runtime and the table above has been updated to include the results from this recent version. 
 
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
- [SeaRange AIS receiver](https://www.shipxplorer.com/searange-ais-receiver)
- [Seattle Capitol Hill, US](https://www.marinetraffic.com/en/ais/details/stations/14916)
- [Troguarat, France](https://www.marinetraffic.com/en/ais/details/stations/21360)
- [Tyres, Sweden](https://www.marinetraffic.com/en/ais/details/stations/22269)
- [Vancouver West End, Canada](https://www.marinetraffic.com/en/ais/details/stations/7029)
- [Vasa, Finland](https://www.marinetraffic.com/en/ais/details/stations/14994)
- [Vernouillet, France](https://www.marinetraffic.com/en/ais/details/stations/10668)
- [Vlissingen, NL](https://www.marinetraffic.com/nl/ais/details/stations/17133)
- [Wren Road Rab 2](https://www.marinetraffic.com/en/ais/details/stations/9615)

## Build process

### Windows Binaries

Links to fully built Windows binaries of recent releases are provided in below table, with and without SDRPlay support (which requires a running SDRPlay API). 
Running ``AIS-catcher`` should be a simple matter of unpacking the ZIP file in one directory and start the executable on the command line with the required parameters or by clicking ``start.bat`` which you can edit with Notepad to set desired parameters.
**There was an issue with v0.42 on Windows for UDP communication. To avoid this problem use an earlier version or the Bleeding Edge**.
It will likely run out of the box in case you have already RTL-SDR software running on your PC. In case you encounter an issue, you might want to check:
- installation of RTL-SDR drivers is done via [Zadig](https://www.rtl-sdr.com/tag/zadig/)
- installation of the Visual Studio runtime [libraries](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).

Recent releases:
 | Version | Win32  | x64 |  Win32 + SDRPlay | x64 + SDRPlay | 
 | :--- | :--- | :---: |   :--- | :---: |     
 |Edge| [ZIP](https://github.com/jvde-github/AIS-catcher/blob/462671222a668b312d5b5e17787d6b24fcd72d14/Edge/AIS-catcher%20x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/blob/462671222a668b312d5b5e17787d6b24fcd72d14/Edge/AIS-catcher%20x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/blob/462671222a668b312d5b5e17787d6b24fcd72d14/Edge/AIS-catcher%20SDRPLAY%20x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/blob/462671222a668b312d5b5e17787d6b24fcd72d14/Edge/AIS-catcher%20SDRPLAY%20x64.zip) |
 |v0.41| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.41/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.41/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.41/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.41/AIS-catcher.SDRPLAY.x64.zip) |
 |v0.40| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.40b/AIS-catcher.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.40b/AIS-catcher.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.40b/AIS-catcher.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.40b/AIS-catcher.SDRPLAY.x64.zip) | 
 |v0.39| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.39/AIS-catcher.Edge.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.39/AIS-catcher.Edge.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.39/AIS-catcher.Edge.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.39/AIS-catcher.Edge.SDRPLAY.x64.zip) | 
|v0.38| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.38/AIS-catcher.v0.38.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.38/AIS-catcher.v0.38.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.38/AIS-catcher.v0.38.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.38/AIS-catcher.v0.38.SDRPLAY.x64.zip) | 
  |v0.37| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.37/AIS-catcher.v0.37.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.37/AIS-catcher.v0.37.x64.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.37/AIS-catcher.v0.37.SDRPLAY.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.37/AIS-catcher.v0.37.SDRPLAY.x64.zip) | 
 |v0.36| [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.36a/AIS-catcher.v0.36a.x86.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/releases/download/v0.36a/AIS-catcher.v0.36a.x64.zip) | | |
   |v0.35| [ZIP](https://github.com/jvde-github/AIS-catcher/raw/media/AIS-catcher%20v0.35%20Win32.zip) | [ZIP](https://github.com/jvde-github/AIS-catcher/raw/media/AIS-catcher%20v0.35%20x64.zip) | | | 
  |v0.34| [ZIP](https://drive.google.com/file/d/1ivz0Pk1KsGfnq5k0E723nXUGfz79ya-d/view?usp=sharing) | [ZIP](https://drive.google.com/file/d/1yfjEnY9fqS6ifmqaatl3EzISdhliIk-j/view?usp=sharing) | | |
 |v0.33 |  [ZIP](https://drive.google.com/file/d/1KFvvWQi47QquOl-jDPRK8mpmUnfQaM91/view?usp=sharing) | [ZIP](https://drive.google.com/file/d/1oE0rTMU7DF9pFzw2Pt1SAMDm-UWILJrT/view?usp=sharing)  |  | |
 
If you are looking for a Windows-version for the latest development version, it is automatically produced by the standard workflow (see Actions).

### Ubuntu, Raspberry Pi, macOS, MSVC
The steps to compile AIS-catcher for RTL-SDR dongles are fairly straightforward on most systems. There are various options including a standard Makefile, a ```solution``` file for MSVC (see next section) and you can use ```cmake```, as we will detail now.

First step is to ensure you have the necessary dependencies and build tools installed for your device(s). 
For example, the following installs the minimum build tools for Ubuntu and Raspberry Pi:
```console
sudo apt-get update
sudo apt-get upgrade

sudo apt-get install git make gcc g++ cmake pkg-config -y
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
***HTTP post***             | libcurl4-openssl-dev zlib1g-dev | | curl curl:x64-windows | X |

Once the dependencies are in place, the process to install AIS-catcher then on Linux based systems becomes:
```console
git clone https://github.com/jvde-github/AIS-catcher.git
cd AIS-catcher
mkdir build
cd build
cmake ..
make
sudo make install
```
For the SDRPlay the software needs to be downloaded and installed from the website of the manufacturer. Once installed, the AIS-catcher build process automatically includes it in the build if available. 

For Windows, clone the project and open the directory with AIS-catcher in Visual Studio 2019 or above. The ```cmake``` file provides two options as source for the libraries. The first is to install all the drivers via PothosSDR from [here](https://downloads.myriadrf.org/builds/PothosSDR/).  The cmake file will locate the installation directory and link against these libraries. The alternative is to use ```vcpkg``` which currently only offers the libraries for RTL-SDR and ZeroMQ (see next section as well). Of course, you can save yourself the hassle and download the Windows binaries from above.

### Running as a service on Ubuntu and Raspberry Pi

Github user abcd567a has developed a nice [script](https://github.com/abcd567a/install-aiscatcher) and manual to automatically build AIS-catcher and set it up as a background service. I tested it on Ubuntu and advice to first systematically identify the optimal settings as described above starting with ``-s 1536K -gr tuner auto rtlagc on -a 192K``. It is paramount that the settings are edited:
```
sudo nano /usr/share/aiscatcher/aiscatcher.conf 
```

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
    command: <ais-catcher command line options>
    container_name: ais-catcher
    devices:
      - "/dev/bus/usb:/dev/bus/usb"
    image: ghcr.io/jvde-github/ais-catcher:latest
    restart: always
```
Please note that the SDRplay devices are currently not supported in the Docker images.

## Considerations

### A note on device sample rates
AIS-catcher automatically sets an appropriate sample rate depending on your device but provides the option to overwrite this default using the ```-s``` switch. For example for performance reasons you can decide to use a lower rate or improve the sensitivity by picking a higher rate than the default. The decoding model supports the following rates:
```
96K, 192K, 288K, 384K, 576K, 768K, 1152K, 1536K, 2304K, 3072K, 6144K, 12288K 
```
Before splitting the signal in two separate signals for channel A and B, AIS-catcher downsamples the signal to 96K samples/second by successively decimating the signal by a factor 2 and/or 3. 
Input on all other sample rates is upsampled to a nearby higher rate to make it fit in this computational structure. Hence, there is no efficiency advantage of using these other rates.
In recent versions of AIS-catcher you can use the ``SOXR`` or ``libsamplerate`` (SRC) library for downsampling. In fact, you can compare the four different downsampling approaches with a command like:
```
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
Note that some libraries will require significant hardware resources. The advice is to use the native build-in downsampling functionality.

The default downsampler uses a simple but efficient CIC5 filter. To mitigate some of the drawbacks of this method, version 0.39 onwards  uses by default  a simple droop compensator in the form of a fast 3 tap filter which can be switched off with the switch ``-go DROOP off``. 
The following results are from my home station running for a few hours with the various methods running in parallel and counting number of messages:

| Downsampler | RTL-SDR @ 1536K  | AirSpy HF+ @ 192K  | SDRPlay RSPdx @ 3072K | 
| :--- | :--- | :--- | :-- | 
|``-go DROOP off``	| 94219 |16022 | 16530 |
|``-go DROOP on`` (default) | 98176 (+4.20%) | 16265 (+1.52%) | 17190 (+3.99%) |
|``-go SOXR on`` (SOX downsampling)	| 97652 (+3.64%) | 16209 (+1.17%) | 17049 (+3.14%) |

For reference, the command line instruction to test is:
```console
AIS-catcher  -v 10 -gr rtlagc on -m 2 -go droop off -m 2 -m 2 -go soxr on
```
Please note that the runs are performed on different days over different time spans so this does not represent a comparison of devices.

### Frequency offset
AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. [see](https://github.com/steve-m/kalibrate-rtl) and provide as input via the ```-p``` switch on the command line.

If you are using a cheap RTL-SDR dongle that suffers from thermal drift (i.e. the required PPM correction drifts when the dongle is getting warmer), you could consider to use the option ``-o AFC_WIDE on`` or switch to a FM decoding model which is less sensitive for frequency offsets (``-m 0``). The frequency correction applied by the default decoding model can be made visible with the switch ``-M D`` so you can inspect.

### System USB performance
On some laptops we observed that Windows was struggling with high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is worthwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
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

- Improve the documentation listing all options (command line and JSON)
- Decoding: further model improvements e.g. using other filters, alternative freq correction models, software gain control
- Add tool to compare different receivers (more statistics than just looking at message count)
- Testing: assess gap with commercial equipment (partially done at Meteotoren)
- Support NMEA tag blocks for timestamp
- Solve CMake issue with zlib on MACOS
- RSSI refinement (measure base noise level), in general add more diagnostics to assess performance issues, e.g. auto ppm calibration
- Option to record raw input signal periodically to allow for debugging of performance
- Simultaneously receive Marine VHF audio and DSC signals from SDR input signal
- Implement websocket interface, store/write configuration files (JSON)
- Channel AB+CD for devices with high sample rates like the Airspy
- Optional filter for invalid messages, optional downsampling messages for HTTP postings
- System support and GUI: Windows, <del>Android</del>, Web interface
- Multi-channel SDRs: validate location from signal (e.g. like MLAT or using passive radar with krakensdr)
- Output: ZeroMQ, <del>APRS, JSON over HTTP,</del> TCP, ...
- <del>NMEA input: check checksum, use fillbits to set length and more tight initial parser</del>
- <del>Allow for verbose updates even if running from stdin</del>
- <del>Adding additional messages to the JSON decoder (Message 8, 6, etc).</del>
- <del>(Unit) testing of the JSON decoder</del>
- <del>Show incremental message count between verbose updates</del> 
- <del>SpyServer support</del>
- <del>Reporting signal strength per message and estimated frequency correction (e.g. to facilitate auto calibration ppm for rtl sdr dongles)</del>
- <del>Resolving crash when Airspy HF+ is disconnected, does not seem to be a specific AIS-catcher issue.</del> Use latest airspyhf lib.
- <del>RTL-TCP setting for timeout on connection (system default takes way too long)</del>
- Input: <del>ZeroMQ/TCP-IP protocols, SoapySDR, SpyServer</del>, LimeSDR mini, ...
- ....
