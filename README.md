# AIS-catcher - A multi-platform AIS receiver
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL-SDR dongles, AirSpy and the Airspy HF+. Output is send in the form of NMEA messages to either screen or broadcasted over UDP. The program provides the option to read and decode the raw discriminator output of a VHF receiver as well.

```AIS-catcher```  is created for research and educational purposes under the MIT license. It is a hobby project from an unqualified amateur and not tested and designed for reliability and correctness. You can play with the software but it is the user's responsibility to use it prudently. So, in summary, DO NOT rely upon this software in any way including for navigation and/or safety of life or property purposes.

## Recent Developments

Release version 0.19: Added Airspy support

Release version 0.18: Update to Dockerfile to accomodate Airspy HF+

## Purpose

The aim of ```AIS-catcher``` is to provide a platform to facilitate continuous improvement of receiver models. Any suggestions, observation or sharing of recordings for setups where the current models are struggling is highly appreciated! The algorithm behind the default receiver model was created in this way by investigating signals and trying different ways to get a coherent model running whilst keeping it simple at the same time. If I have some more free time I will try to expand the documentation and implement some improvement ideas.

## Usage
````
use: AIS-catcher [options]

	[-h display this message and terminate (default: false)]
	[-s xxx sample rate in Hz (default: based on SDR device)]
	[-v [option: xx] enable verbose mode, optional to provide update frequency in seconds (default: false)]
	[-q surpress NMEA messages to screen (default: false)]
	[-u address port - UDP address and port (default: off)]

	[-r filename - read IQ data from raw 'unsigned char' file]
	[-r cu8 filename - read IQ data from raw 'unsigned char' file]
	[-r cs16 filename - read IQ data from raw 'signed 16 bit integer' file]
	[-r cf32 filename - read IQ data from WAV file in 'float' format]
	[-w filename - read IQ data from WAV file in 'float' format]

	[-l list available devices and terminate (default: off)]
	[-d:x select device based on index (default: 0)]
	[-d xxxx select device based on serial number]

	[-p xx frequency correction for RTL SDR]

	[-gm Airspy gain control in form SENSITIVITY/LINEARITY/VGA/LNA/MIXER followed by gain level or "auto" where applicable]

	[-m xx run specific decoding model (default: 2)]
	[	0: Standard (non-coherent), 1: Base (non-coherent), 2: Default, 3: FM discrimator output]
	[-b benchmark demodulation models - for development purposes (default: off)]
````

## Examples


To test a proper installation and/or compilation (see below), we can first try to run the program on a RAW audio file as in this [tutorial](https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48000 -r cs16 helsinki-210-messages.raw
```
AIS-catcher on this file should extract roughly ``360`` AIVDM lines. Notice that with switch ```-m 3``` on the command line AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case the program is similar to the following usage of GNUAIS:
```console
gnuais -l helsinki-210-messages.raw
```
which produces:
```console
INFO: A: Received correctly: 153 packets, wrong CRC: 49 packets, wrong size: 4 packets
INFO: B: Received correctly: 52 packets, wrong CRC: 65 packets, wrong size: 10 packets
```

To list the devices available for AIS reception:
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
AIS-catcher -v 10 -u 127.0.0.1 12345
```
If succesful, NMEA messages will start to come in, appear on the screen and send as UDP messages to `127.0.0.1` port `12345`. These console messages can be surpressed with the option ```-q```.

The following commands record a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and then subsequently decodes the input with AIS-catcher:
```console
rtl_sdr -s 288K -f 162M  test_288.raw
AIS-catcher -r test_288.raw -s 288000 -v
```
## Examples of device specific settings

The command line allows you to set some device specific parameters. Below some examples.

### Airspy Mini

The Airspy Mini requires careful gain configuration as described [here](https://airspy.com/quickstart/). As described in the reference there are three few different gain modes: linearity, sensitivity and free. These can be set via the ```-gm```switch when using the Airspy Mini. We can activiate 'linearity' mode with gain ```10```using:
```
AIS-catcher -gm LINEARITY 10
```
Similar for 'sensitivity'  mode:
```
AIS-catcher -gm SENSITIVITY 10
```
Finally, full control is obtained with the command:
```
AIS-catcher -gm LNA auto VGA 12 MIXER 12
```
More guidance on setting the gain model and levels can be obtained from the reference.

## Multiple receiver models

The command line provides  the ```-m``` option which allows for the selection of the specific receiver models.  In the current version 4 different receiver models are embedded:

- **Default model** (``-m 2``): the default demodulation engine
- **Base model (non-coherent)** (``-m 1``): similar to RTL-AIS (and GNUAIS/Aisdecoder) with some modifications to PLL and main receiver filter (taken from [here](https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-designing.html) and [here](https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-improving-gnuais.html) ).
- **Standard model (non-coherent)** (``-m 0``): as the base model with brute force timing recovery, described [here](https://jaspersnotebook.blogspot.com/2021/07/ais-catcher-dual-channel-ais-receiver.html).
- **FM discriminator model**: (``-m 3``) as  the 'standard' model but assumes input is output of a FM discriminator, hence no FM demodulation takes place which allows ```AIS-catcher``` to be used as GNUAIS and AISdecoder.

The default model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station it improves message count by a factor 2 - 3. The reception quality of the `standard` model over the `base` model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the default model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```-m 1```, if needed.

 Notice that you can execute multiple models in one run for benchmarking purposes but only the messages from the first model specified are displayed and forwarded. To benchmark different models specify ```-b``` for timing and/or ```-v``` to compare message count, e.g.
```
AIS-catcher -s 1536000 -r posterholt_1536_2.raw -m 2 -m 0 -m 1 -q -b -v
```
The program will run and summarize the performance (count and timing) of three decoding models:
```
[AIS engine v0.13+]	        : 38 msgs at 43 msg/s
[Standard (non-coherent)]	: 4 msgs at 4.6 msg/s
[Base (non-coherent)]	        : 3 msgs at 3.4 msg/s
```
```
[AIS engine v0.13+]	        : 3.5e+02 ms
[Standard (non-coherent)]	: 2.2e+02 ms
[Base (non-coherent)]	        : 2e+02 ms
```
In this example the default model performs quite well in contrast to the standard non-coherent engine with 38 messages identified versus 4 for the standard engine. This is typical when there are few messages with poor quality. However, it  doubles the decoding time and has a higher memory usage so needs more powerful hardware. Please note that the improvements seen for this particular file are an exception.

## Validation

To get a sense of the performance of the different models, I have run a simple test in two different setups whereby ```AIS-catcher``` ran the main two models in parallel for 2 minutes and we counted the number of detected messages.

Location: The Hague residential area with RTL-SDR v3 dongle and Shakespeare antenna with quite some (perhaps fair to say a lot of) blockage from surrounding buildings and antenna placed within a window, we have the following message count for various models and sample rates (over 2 minute run):

 | Model | Settings | Run 1 | Run 2 | 
 | :--- | :--- | :---: | :---: |
 | AIS-catcher v0.13 Default @ 1536K  |  | 191 | 194 | 
| AIS-catcher Standard (non-coherent) @ 1536K |```-m 0``` |   64 | 72 | 
| AISRec 2.2 (trial)   | Sampling: very high, super fast| 161 | 119 |
| rtl-ais v0.3  @ 1600K  | ```-n``` | 33 | 30  | 

For completeness I performed seperate runs with [AISRec](https://sites.google.com/site/feverlaysoft/home) and [RTL-AIS](https://github.com/dgiardini/rtl-ais) as well. Results for a [dAISy HAT](http://www.wegmatt.com/) are not listed but in this setup I received roughly ~85 messages over a two minute run.

## Releases

A release in binary format for Windows 32 bit (including required libraries) can be occasionally found with the latest release but if not, please reach out and I will make it available. Note that you will have to install drivers using Zadig (https://www.rtl-sdr.com/tag/zadig/). After that, simply unpack the ZIP file in one directory and start the executable. For Linux systems, compilation instructions are below.

## Compilation process for Linux/Raspberry Pi

Make sure you have the following dependencies, depending on the device you are using:
  - librtlsdr
  - libairspyhf
  - libairspy

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

If you want to include Airspy and Airspy HF+ functionality, ensure you install the required libraries as descibed on https://github.com/airspy/airspyhf and https://github.com/airspy/airspyone_host. If not installed yet, you might first want to try:
```console
sudo apt-get install libairspyhf-dev libairspy-dev
```
The process to install AIS-catcher then becomes:
```console
make
sudo make install
```

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

## Considerations

AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. [see](https://github.com/steve-m/kalibrate-rtl) and provide as input via the ```-p``` switch on the command line.

On some laptops we observed that Windows was struggling with high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is wortwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
```console
rtl_test -s 1536000
```
In case you observe a high number of lost data, the advice is to run AIS-catcher at a lower sampling rate for RTL SDR dongles:
```console
AIS-catcher -s 288000
```
If your system allows for it you might opt to run ```AIS-catcher``` at a sample rate of ```2304000```. 

## To do

- Access to SDR hardware specific functionality, e.g. gain control
- Documenting and finetuning the default decoding model
- Ongoing: further improvements to reception and testing (e.g. improve coherent demodulation, filters, etc)
- Performance improvements
- More testing: in particular in an area with high message density
- Windows GUI
- ....
- ....
