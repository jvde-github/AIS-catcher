# AIS-catcher - A multi-platform AIS receiver
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL-SDR dongles, AirSpy Mini, Airspy R2, Airspy HF+, HackRF, SDRplay (RSP1A for now) and input from file and over RTL TCP. Output is send in the form of NMEA messages to either screen or broadcasted over UDP. 
The program provides the option to read and decode the raw discriminator output of a VHF receiver as well. 

![Image](https://1.bp.blogspot.com/-YUzJiP0K_38/YE352lEPPjI/AAAAAAAAAA0/7DRYSu18NJAb9U6mLDTvqxOftAR_zyKaQCLcBGAsYHQ/s2689/containership.jpg)

### Disclaimer

```AIS-catcher```  is created for research and educational purposes under the MIT license. It is a hobby project and not tested and designed for reliability and correctness. You can play with the software but it is the user's responsibility to use it prudently and in line with local regulations and laws. So, in summary, DO NOT rely upon this software in any way including for navigation and/or safety of life or property purposes.

## Recent Developments

For testing, do not use the development version (edge) but instead download the latest release. The development version might not work. 

The **edge** version:
- restructuring of the directory layout
- Makefile now autodetects library locations: ```make``` will only build for installed SDR libraries
- recalibration of decoding parameters resulting in a small improvement in sensitivity
- added [instructions](https://github.com/jvde-github/AIS-catcher#microsoft-visual-studio-2019-rtl-sdr-only) and a solution file for building AIS-catcher with RTL-SDR support on Windows using Visual Studio 2019 and above (at a few requests) 
- proper call to close devices at the end of the program. Unfortunately, on Windows 10 this can result in a crash. However, this is hopefully is fixed in the latest version of the [rtl-sdr library](https://github.com/osmocom/rtl-sdr/commit/2659e2df31e592d74d6dd264a4f5ce242c6369c8). I will provide this version with the Windows binaries below.

**Release version 0.32**: Support for the **Raspberry Pi Model B Rev 2** via performance enhancements at the cost of a  small tradeoff in sensitivity. 
I implemented a trick to speed up fixed point downsampling for RTLSDR input at 1536K samples/second. Furthermore a new model (```-m 5```) is introduced  which uses exponential moving averages in the determination of the phase instead of a standard moving average as for the default model.
Both features can be activated with the ```-F``` switch. 
To give an idea of the performance improvement on a Raspberry PI (700 MHz), I used the following command to decode from a file on the aforementioned Raspberry Pi:

```
AIS-catcher -r posterholt.raw -s 1536000 -b -q -v
```
Resulting in 38 messages and the ```-b``` switch prints the timing used for decoding:
```
[AIS engine v0.31]	: 17312.1 ms
```
Adding the ```-F``` switch yielded the same number of messages but timing is now:
```
[AIS engine (speed optimized) v0.31]	: 7722.32 ms
```
This and other performance updates make the full version of AIS-catcher run on an early version of the Raspberry Pi with very limited drops.

Release version **0.31**: allow input from stdin and very minor speed and performance improvements

## Purpose

The aim of ```AIS-catcher``` is to provide a platform to facilitate continuous improvement of receiver models. Any suggestions, observation or sharing of recordings for setups where the current models are struggling is highly appreciated! The algorithm behind the default receiver model was created in this way by investigating signals and trying different ways to get a coherent model running whilst keeping it simple at the same time. If I have some more free time I will try to expand the documentation and implement some improvement ideas.

## Installation and Windows Binary

Building instructions are provided below for most systems..

A Windows binary version of **v0.32** (ex SDRplay support) is available for [32-BIT](https://drive.google.com/file/d/1nMftfB1XsRBXHRTQ3kS8e3TTIN-fm12a/view?usp=sharing) and [64-BIT](https://drive.google.com/file/d/1-lBCfFejeZEl1-wXi_-_S6nBMNqIeQjA/view?usp=sharing) from my Google Drive. If you did not access these files before I might have to give you access. Furthermore, note that you will have to install drivers using Zadig (https://www.rtl-sdr.com/tag/zadig/). After that, simply unpack the ZIP file in one directory and start the executable on the command line with the required parameters.

If you are looking for a Windows binary supporting SDRplay API 3.09, please get in contact with [me](mailto:jvde.github@gmail.com).

## Usage
````
use: AIS-catcher [options]

	[-h display this message and terminate (default: false)]
	[-s xxx sample rate in Hz (default: based on SDR device)]
	[-v [option: 1+] enable verbose mode, optional to provide update frequency in seconds (default: false)]
	[-q suppress NMEA messages to screen (default: false)]
	[-n show NMEA messages on screen without detail]
	[-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]

	[-r [optional: yy] filename - read IQ data from file, short for -r -ga FORMAT yy FILE filename, for stdin input use filename equals stdin or .]
	[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]
	[-t [host [port]] - read IQ data from remote RTL-TCP instance]

	[-l list available devices and terminate (default: off)]
	[-L list supported SDR hardware and terminate (default: off)]
	[-d:x select device based on index (default: 0)]
	[-d xxxx select device based on serial number]

	[-m xx run specific decoding model (default: 2), see README for more details]
	[-b benchmark demodulation models for time - for development purposes (default: off)]
	[-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]

	Device specific settings:

	[-gr RTLSDRs: TUNER [auto/0.0-50.0] RTLAGC [on/off] BIASTEE [on/off] FREQOFFSET [-150-150]
	[-p xx equivalent to -gr FREQOFFSET xx]
	[-gm Airspy: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]
	[-gh Airspy HF+: TRESHOLD [low/high] PREAMP [on/off] ]
	[-gs SDRPLAY: GRDB [0-59] LNASTATE [0-9] AGC [on/off] ]
	[-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off]
	[-gt RTLTCPs: HOST [address] PORT [port] TUNER [auto/0.0-50.0] RTLAGC [on/off] FREQOFFSET [-150-150]
	[-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8]
	[-gw WAV file: FILE [filename]
````

## Examples


To test a proper installation and/or compilation (see below) we can list the devices available for AIS reception:
```console
AIS-catcher -l
```
The output depends on the available devices but will look something like:
```console
Found 1 device(s):
0: Realtek, RTL2838UHIDIR, SN: 00000001
```
A specific device can be selected with the ``d``-switch like ``-d:0`` or ``-d 00000001``.


To start AIS demodulation, print some occasional statistics (every 10 seconds) and broadcast AIS messages via UDP, we can use the following command:
```console
AIS-catcher -v 10 -u 127.0.0.1 12345 -u 127.0.0.1 23456
```
If successful, NMEA messages will start to come in, appear on the screen and send as UDP messages to `127.0.0.1` port `12345` and port `23456`. 
These console messages can be suppressed with the option ```-q```. 


The following commands record a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and pipes it to AIS-catcher for decoding:
```console
rtl_sdr -s 288K -f 162M  - | AIS-catcher -r . -s 288000 -v
```
An example of using sox for downsampling the signal and then sending the result to AIS-catcher:
```console
sox -c 2 -r 1536000 -b 8 -e unsigned -t raw posterholt.raw -t raw -b 16 -e signed -r 96000 - |AIS-catcher -s 96000 -r CS16 . -v
```

### Input from FM discriminator
We can run AIS-catcher on a RAW audio file as in this [tutorial](https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48000 -r cs16 helsinki-210-messages.raw
```
On this file we should extract roughly ``360`` AIVDM lines. Notice that with switch ```-m 3``` on the command line AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case the program is similar to the following usage of GNUAIS:
```console
gnuais -l helsinki-210-messages.raw
```
which produces:
```console
INFO: A: Received correctly: 153 packets, wrong CRC: 49 packets, wrong size: 4 packets
INFO: B: Received correctly: 52 packets, wrong CRC: 65 packets, wrong size: 10 packets
```

## Device specific settings

The command line allows you to set some device specific parameters. AIS-catcher follows the settings and naming conventions for the devices as much as possible so that parameters and settings determined by SDR software for signal analysis (e.g. SDR#, SDR++, SDRangel) can be directly copied. Below some examples.

### RTL SDR
Gain and other settings specific for the RTL SDR can be set on the command line with the ```-gr``` switch. For example, the following command sets the tuner gain to +33.3 and the RTL AGC on:
```console
AIS-catcher -gr tuner 33.3 rtlagc ON
```
Please note that these settings are not case sensitive.

### Airspy HF+
Gain settings specific for the Airspy HF+ can be set on the command line with the ```-gh``` switch. For example, the following command switches on the preamp:
```console
AIS-catcher -gh preamp OFF
```
Please note that only AGC mode is supported.

### Airspy Mini/R2

The Airspy Mini and R2 require careful gain configuration as described [here](https://airspy.com/quickstart/). As outlined in that reference there are three different gain modes: linearity, sensitivity and so-called free. These can be set via the ```-gm```switch when using the Airspy. We can activate 'linearity' mode with gain ```10```using the following ```AIS-catcher``` command line:
```console
AIS-catcher -gm linearity 10
```
Finally, gains at different stages can be set as follows:
```console
AIS-catcher -gm lna AUTO vga 12 mixer 12
```
More guidance on setting the gain model and levels can be obtained in the mentioned reference.

### SDRplay RSP1A (API 3.x)
Settings specific for the SDRplay RSP1A can be set on the command line with the ```-gs``` switch, e.g.:
```console
AIS-catcher -gs lnastate 5
```

### HackRF
Settings specific for the HackRF can be set on the command line with the ```-gf``` switch, e.g.:
```console
AIS-catcher -gf lna 16 vga 16 preamp OFF
```

### RTL TCP
AIS-catcher can process the data from a [`rtl_tcp`](https://projects.osmocom.org/projects/rtl-sdr/wiki/Rtl-sdr#rtl_tcp) process running remotely, e.g. if the server is on `192.168.1.235` port `1234` with a sampling rate of `240K` samples/sec:
```console
AIS-catcher -t 192.168.1.235 1234 -s 240000 -v
```

## Multiple receiver models

The command line provides  the ```-m``` option which allows for the selection of the specific receiver models.  In the current version 4 different receiver models are embedded:

- **Default model** (``-m 2``): the default demodulation engine
- **Base model (non-coherent)** (``-m 1``): similar to RTL-AIS (and GNUAIS/Aisdecoder) with some modifications to PLL and main receiver filter (taken from [here](https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-designing.html) and [here](https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-improving-gnuais.html) ).
- **Standard model (non-coherent)** (``-m 0``): as the base model with brute force timing recovery, described [here](https://jaspersnotebook.blogspot.com/2021/07/ais-catcher-dual-channel-ais-receiver.html).
- **FM discriminator model**: (``-m 3``) as  the 'standard' model but assumes input is output of a FM discriminator, hence no FM demodulation takes place which allows ```AIS-catcher``` to be used as GNUAIS and AISdecoder.

The default model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station it improves message count by a factor 2 - 3. The reception quality of the `standard` model over the `base` model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the default model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```-m 1```, if needed.

 Notice that you can execute multiple models in one run for benchmarking purposes but only the messages from the first model specified are displayed and forwarded. To benchmark different models specify ```-b``` for timing and/or ```-v``` to compare message count, e.g.
```console
AIS-catcher -s 1536000 -r posterholt.raw -m 2 -m 0 -m 1 -q -b -v
```
The program will run and summarize the performance (count and timing) of three decoding models (on a Raspberry Pi 4B):
```
[AIS engine v0.31]		: 38 msgs at 6.2 msg/s
[Standard (non-coherent)]	: 4 msgs at 0.7 msg/s
[Base (non-coherent)]		: 3 msgs at 0.5 msg/s
```
```
[AIS engine v0.31]		: 1139.2 ms
[Standard (non-coherent)]	: 900.099 ms
[Base (non-coherent)]		: 837.641 ms
```
If we use the ```-F``` switch to enable fixed point downsampling, the results are the same but the timings become:
```
[AIS engine v0.31]		: 891.025 ms
[Standard (non-coherent)]	: 638.765 ms
[Base (non-coherent)]		: 586.179 ms
```
In this example the default model performs quite well in contrast to the standard non-coherent engine with 38 messages identified versus 4 for the standard engine. This is typical when there are few messages with poor quality. However, it  doubles the decoding time and has a higher memory usage so needs more powerful hardware. Please note that the improvements seen for this particular file are an exception.

## Validation

### Experimenting with recorded signals
The functionality to receive radio input from `rtl_tcp` provides a route to compare different receiver packages on a deterministic input from a file. I have tweaked the callback function in `rtl_tcp` so that it instead sends over input from a file to an AIS receiver like `AIS-catcher` and `AISrec`. The same trick can be easily done for `rtl-ais`. The sampling rate of the input file was converted using `sox` to 240K samples/second for `rtl-tcp` and 1.6M samples/second for `rtl-ais`. The output of the various receivers was sent via UDP to AISdipatcher which removes any duplicates and counts messages. The results in terms of  number of messages/distinct vessels:
 | File | AIS-catcher v0.32  | AIS-catcher EDGE | rtl-ais | AISrec 2.208 (trial - super fast) | AISrec 2.208 (pro - slow2)  | Source |
 | :--- | :--- | :---: | :---: | :---: | :---: | :---: | 
 |Scheveningen |   43/37| 43/37  | 17/16 | 30/27 | 37/31 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Moscow| 197/33 |210/32 | 146/27 |  195/31 |  183/34 | shared by user @ 1920K in [discussion](https://github.com/jvde-github/AIS-catcher/issues/7) |
 |Vlieland | 93/53 |93/53  | 51/31| 72/44 | 80/52 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 |Posterholt |  39/22  |39/22 |2/2 | 13/12 | 31/21 | recorded @ 1536K with `rtl-sdr` (auto gain) |
 
 **Update 1:** AISrec recently had a version update of 2.208 (October 23, 2021) with improved stability and reception quality and the table above has been updated to include the results from this recent version. 

 **Update 2:** Feverlaysoft has kindly provided me with a license for version 2.208 of AISrec allowing access to additional decoding models. Some experimentation suggests that "Slow2" works best for these particular examples and has been included in the above overview.
 
## Build process

### Ubuntu and Raspberry Pi
The steps to compile AIS-catcher for RTL-SDR dongles are fairly straightforward on a Raspberry Pi 4B and Ubuntu systems. First ensure you have the necessary dependencies installed for your device(s). 
If not, the following commands can be used for most devices except for the SDRPLAY:

```console
sudo apt-get update
sudo apt-get upgrade

sudo apt-get install git make gcc g++ librtlsdr-dev libairspyhf-dev libairspy-dev libhackrf-dev pkg-config -y
```
The process to install AIS-catcher then becomes:
```console
git clone https://github.com/jvde-github/AIS-catcher.git
cd AIS-catcher
make
sudo make install
```
Standard installation will include support for the Airspy devices, HackRF and RTLSDR dongles but not SDRplay. To build an executable with SDRplay API 3.x (only) support use:
```console
make sdrplay-only
sudo make install
```
At the moment only RSP1A is supported (as that is the only device I can test on).

### Microsoft Visual Studio 2019+ (RTL-SDR only)

Ensure that you have ```vcpkg``` [installed](https://vcpkg.io/en/getting-started.html) and integrated into Visual Studio via ```vcpkg integrate install``` (as Administrator). Then install the rtl-sdr drivers as follows:
```
vcpkg install rtlsdr rtlsdr:x64-windows
```
The included solution file in the mscv directory should allow you to build AIS-catcher with RTL-SDR support in the Visual Studio IDE.

### Mac OS X (RTL-SDR only)
The following shows the installation instructions for RTL-SDR dongles. First ensure you install the necessary dependencies:
```console
brew update
brew install librtlsdr pkg-config
````
Building AIS-catcher is similar to Linux:
```console
git clone https://github.com/jvde-github/AIS-catcher.git
cd AIS-catcher
make rtl-only
sudo make install
```

### Additional compilation options
The make process allows for additional compilation options to be set at the command line via defining CFLAGS, e.g.: 
```
make CFLAGS='-DLIBRTLSDR_LEGACY' rtl-only
```
Some useful options are: 

 | Description | CFLAGS | Impact |
 | :--- | :--- | :--- |
 |use librtlsdr version 5.x | -DLIBRTLSDR_LEGACY | Allows compilation with librtlsdr v5.x |
 |Linux x64 performance turning  | -D-march=native | ~ 5% < decoding time |

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

AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. [see](https://github.com/steve-m/kalibrate-rtl) and provide as input via the ```-p``` switch on the command line.

On some laptops we observed that Windows was struggling with high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is worthwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
```console
rtl_test -s 1536000
```
In case you observe a high number of lost data, the advice is to run AIS-catcher at a lower sampling rate for RTL SDR dongles:
```console
AIS-catcher -s 288000
```
If your system allows for it you might opt to run ```AIS-catcher``` at a sample rate of ```2304000```. 

## Known issues

- call of ```rtlsdr_close```  in MS VC++ can result in a crash. To be investigated further but looks simple tweak in librtlsdr can solve the problem, see [rtlsdr](https://github.com/osmocom/rtl-sdr/pull/18).
- ...

## To do

- DSP: improve filters (e.g. add droop compensation, large rate reductions), etc
- Decoding: add new improved models (e.g. using matched filters, alternative freq correction models), software gain control, document current model
- Testing: more set ups, assess gap with commercial equipment
- GUI: Windows, Android
- Multiple SDRs: validate location from signal (e.g. like MLAT), privacy filters
- Output: ZeroMQ, APRS, ...
- Input: ZeroMQ, ...
- ....
