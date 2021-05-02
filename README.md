# AIS-catcher - An AIS receiver for RTL-SDR dongles and the Airspy HF+
This package will add the AIS-catcher command which is an AIS receiver for RTL SDR dongles and the Airspy HF+.

```
use: AIS-catcher [options]
	[-s xxx sample rate in Hz (default: based on SDR device)]
	[-d:x device index (default: 0)]
	[-v enable verbose mode (default: false)]
	[-r filename - read IQ data from raw 'unsigned char' file]
	[-w filename - read IQ data from WAV file in 'float' format]
	[-l list available devices and terminate (default: off)]
	[-q surpress NMEA messages to screen (default: false)]
	[-p xx frequency correction for RTL SDR]
	[-u xx.xx.xx.xx yyy UDP address and port (default: off)]
	[-h display this message and terminate (default: false)]
	[-c run challenger model - for development purposes (default: off)]
	[-b benchmark demodulation models - for development purposes (default: off)]

```

Examples
--------

To list the devices available for AIS reception:
```
AIS-catcher -l
```
Wich reports depending on the devices connected, something like:
```
Available devices:
-d:0 AIRSPY HF+  [3652A98081343F89]
```

To start AIS demodulation, print some occasional statistics and broadcast them via UDP, we can use the following command:
```
AIS-catcher -v -u 127.0.0.1 12345
```
or, equivalently,
```
AIS-catcher -d:0 -v -u 127.0.0.1 12345
```
If succesful, NMEA messages will start to come in and appear on the screen. These can be surpressed with the opton ```-q```. 

The following command reads input from an IQ input file recorded with ```rtl_sdr``` at a sampling rate of 288K Hz.
```
AIS-catcher -r Signals/rtl/25042021_288000_1.raw -s 288000 -v -q 
```
The output will be resembling:
```
Frequency     : 162000000
Sampling rate : 288000
----------------------
[AIS Catcher v0.01]	: 34 msgs at 388.054 msg/s
```

Releases
--------
A release in binary format for Windows 64 and 32 bit (including required libraries) can be found as part of the code. For Linux systems, compilation instructions are below. Please note that you will have to install drivers using Zadig (https://www.rtl-sdr.com/tag/zadig/).

Compiling
---------
Make sure you have the following dependencies:
  - librtlsdr and/or libairspyhf
  - libusb
  - libpthread
 
The steps to compile AIS-catcher are as follows:

```console

git clone https://github.com/jvde-github/AIS-catcher.git
cd AIS-catcher
make
./AIS-catcher
```

If you do not have an Airspy HF+ or an RTL-SDR dongle you can replace ```make``` in the above with ```make rtl-only``` or ```make airspyhf-only``` which will remove the dependency on these external libraries.

To do
-----

...

