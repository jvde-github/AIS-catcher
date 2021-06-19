# AIS-catcher - An AIS receiver for RTL-SDR dongles and Airspy HF+
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL SDR dongles and the Airspy HF+. The program provides the option to read and decode the raw discriminator output of a VHF receiver as well. Output is send in the form of NMEA messages to either screen or broadcasted over UDP.

AIS-catcher is created for research and educational purposes. DO NOT rely upon this software for navigation and/or safety of life or property purposes.


```
use: AIS-catcher [options]
	[-s xxx sample rate in Hz (default: based on SDR device)]
	[-d:x device index (default: 0)]
	[-v enable verbose mode (default: false)]

	[-r filename - read IQ data from raw 'unsigned char' file]
	[-r cu8 filename - read IQ data from raw 'unsigned char' file]
	[-r cs16 filename - read IQ data from raw 'signed 16 bit integer' file]
	[-r cf32 filename - read IQ data from WAV file in 'float' format]

	[-w filename - read IQ data from WAV file in 'float' format]
	[-l list available devices and terminate (default: off)]
	[-q surpress NMEA messages to screen (default: false)]
	[-p xx frequency correction for RTL SDR]
	[-u xx.xx.xx.xx yyy UDP address and port (default: off)]
	[-h display this message and terminate (default: false)]
	[-m xx run specific decoding model - 0: standard, 1: base, 2: coherent (default: 0)]
	[-b benchmark demodulation models - for development purposes (default: off)]

Note: if sample rate is set at 48 KHz, input is assumed to be the output of a FM discriminator
```

Examples
--------

To test a proper installation and/or compilation, we can first try to run the program on a RAW audio file as in this tutorial (https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -v -s 48000 -r cs16 helsinki-210-messages.raw
```
AIS-catcher on this file should extract roughly 361 AIVDM lines. Notice that if the sample rate is set at 48 KHz, AIS-catcher assumes that the input is the output of a FM discriminator. In this case the program is similar to the following usage of GNUAIS:
```console
gnuais -l helsinki-210-messages.raw
```

To list the devices available for AIS reception:
```console
AIS-catcher -l
```
Which reports depending on the devices connected, something like:
```console
Available devices:
-d:0 AIRSPY HF+  [3652A98081343F89]
```

To start AIS demodulation, print some occasional statistics and broadcast them via UDP, we can use the following command:
```console
AIS-catcher -v -u 127.0.0.1 12345
```
or, equivalently,
```console
AIS-catcher -d:0 -v -u 127.0.0.1 12345
```
If succesful, NMEA messages will start to come in and appear on the screen. These can be surpressed with the option ```-q```. 

The following command reads input from an IQ input file recorded with ```rtl_sdr``` at a sampling rate of 288K Hz.
```console
AIS-catcher -r Signals/rtl/25042021_288000_1.raw -s 288000 -v -q 
```
The output will be resembling:
```
Frequency     : 162000000
Sampling rate : 288000
----------------------
[AIS Catcher v0.01]	: 34 msgs at 388.054 msg/s
```

Latest Developments
-------------------
The ```-c``` switch has been replaced with the more targeted ```-m``` option which allows the selection of specific decoding models. This has been added to allow for inclusion of a very simple coherent decoding model in the coming weeks which can be selected by running ```-m 2```. Notice that you can select multiple models to allow for benchmarking but only the messages from the first model specified are displayed and forwarded. For benchmarking different models specify ```-b``` for timing or ```-v``` to compare message count.

Releases
--------
A release in binary format for Windows 32 bit (including required libraries) can be found for your convenience as part of the release section (AIS-catcher W32.zip). Please note that you will have to install drivers using Zadig (https://www.rtl-sdr.com/tag/zadig/). After that, simply unpack the ZIP file in one directory and start the executable. For Linux systems, compilation instructions are below. 

Compilation process for Linux/Raspberry Pi
------------------------------------------

Make sure you have the following dependencies:
  - librtlsdr and/or libairspyhf
 
The steps to compile AIS-catcher for RTL-SDR dongles are fairly straightforward on a Raspberry Pi 4B and Ubuntu systems. First ensure you have the necessary dependencies installed. If not, the following commands can be used:

```console
sudo apt-get update
sudo apt-get upgrade

sudo apt-get install git make gcc g++ -y
sudo apt-get install librtlsdr-dev -y

```

Next step is to download AIS-catcher source and install the program:

```console

git clone https://github.com/jvde-github/AIS-catcher.git
cd AIS-catcher
make rtl-only
sudo make install
```

If you want to include Airspy HF+ functionality, ensure you install the required libraries as descibed on https://github.com/airspy/airspyhf. The process to install AIS-catcher then becomes:
```console
make
sudo make install
```
Comments
--------

AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. using https://github.com/steve-m/kalibrate-rtl and provide as input via the ```-p``` switch on the command line.

On some laptops we observed that Windows was struggling with high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is wortwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
```console
rtl_test -s 1536000
```
In case you observe a high number of lost data, the advice is to run AIS-catcher at a lower sampling rate for RTL SDR dongles:
```console
AIS-catcher -s 288000
```

To do
-----
- Ongoing: further improvements to reception and testing (e.g. add coherent modulation, downsampling, etc)
- Access to hardware specific functionality, e.g. gain control
- Windows GUI
- Ability to select specific receiver engine(s) at the command line
- Overflow detection
- Windows driver improvements
- Automatic frequency correction function
- ....
- ...

