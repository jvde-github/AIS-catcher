# AIS-catcher - An AIS receiver for RTL-SDR dongles and Airspy HF+
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL-SDR dongles and the Airspy HF+. Output is send in the form of NMEA messages to either screen or broadcasted over UDP. The idea behind ```AIS-catcher``` is that it should be easy to run various decoding model in parallel and read input from file to create an environment to test and benchmark different decoding models in a live setting. The program provides the option to read and decode the raw discriminator output of a VHF receiver as well. 

## Purpose
```AIS-catcher``` is created for research and educational purposes under the MIT license. DO NOT rely upon this software in any way including for navigation and/or safety of life or property purposes.

The aim of ```AIS-catcher``` is to provide a platform to facilitate continuous improvement of receiver models. Any suggestions, observation or sharing of recordings for setups where the current models are struggling is highly appreciated! The algorithm behind the default receiver model was created in this way by investigating signals and trying different ways to get a coherent model running whilst keeping it simple at the same time. If I have some more free time I will try to expand the documentation and implement some improvement ideas.

## Usage
````
use: AIS-catcher [options]

	[-h display this message and terminate (default: false)]
	[-s xxx sample rate in Hz (default: based on SDR device)]
	[-v enable verbose mode (default: false)]
	[-q surpress NMEA messages to screen (default: false)]
	[-u xx.xx.xx.xx yyy UDP address and port (default: off)]

	[-r filename - read IQ data from raw 'unsigned char' file]
	[-r cu8 filename - read IQ data from raw 'unsigned char' file]
	[-r cs16 filename - read IQ data from raw 'signed 16 bit integer' file]
	[-r cf32 filename - read IQ data from WAV file in 'float' format]
	[-w filename - read IQ data from WAV file in 'float' format]

	[-l list available devices and terminate (default: off)]
	[-d:x device index (default: 0)]
	[-p xx frequency correction for RTL SDR]

	[-m xx run specific decoding model (default: 2)]
	[	0: Standard (non-coherent), 1: Base (non-coherent), 2: Default, 3: FM discrimator output, 4: challenger model]
	[-b benchmark demodulation models - for development purposes (default: off)]
````

## Examples


To test a proper installation and/or compilation (see below), we can first try to run the program on a RAW audio file as in this [tutorial](https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48000 -r cs16 helsinki-210-messages.raw
```
AIS-catcher on this file should extract roughly ``360`` AIVDM lines. Notice that if the sample rate is set at 48 KHz with switch ```-m 3```, AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case the program is similar to the following usage of GNUAIS:
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
Available devices:
-d:0 AIRSPY HF+  [3652A98081343F89]
```

To start AIS demodulation, print some occasional statistics and broadcast them via UDP, we can use the following command:
```console
AIS-catcher -v -u 127.0.0.1 12345
```
If succesful, NMEA messages will start to come in, appear on the screen and send as UDP messages to `127.0.0.1` port `12345`. These console messages can be surpressed with the option ```-q```. 

The following commands record a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and then subsequently decodes the input with AIS-catcher:
```console
rtl_sdr -s 288K -f 162M  test_288.raw
AIS-catcher -r test_288.raw -s 288000 -v 
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

## Multiple receiver models

In the current version 4 different receiver models are embedded:

- `Default model`: a simple coherent demodulation model that tries to make local estimates of the phase offset. The idea was to find a balance between the reception quality of coherent models and robustness of non-coherent model towards frequency and phase offsets. 
- `Base model (non-coherent)`: base model similar to RTL-AIS (and GNUAIS/Aisdecoder) with some modifications to PLL and main receiver filter ([taken from here](https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-designing.html)).
- `Standard model (non-coherent)`: as the base model with more aggressive PLL
- `FM discriminator model`: as  the 'standard' model but assumes input is output of a FM discriminator, hence no FM demodulation takes place which allows ```AIS-catcher``` to be used as GNUAIS and AISdecoder.

The default model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station it improves message count by a factor 2 - 3. The reception quality of the `standard` model over the `base` model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the default model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```-m 1``` if needed.

To get a sense of the performance of the different models, I have run a simple test in two different setups whereby ```AIS-catcher``` ran the three models in parallel for 5 minutes and we counted the number of detected messages. Due to the USB issues I have on my laptop for Windows (as described in a previous section), I have ran on Windows at a low sampling rate of 288K samples per second.

Location: Vlieland with NESDR RTL-SDR dongle with factory included antenna (with sampling rate and system as per table) gives the following message count (5 minute run):
 | Model | Settings | Run 1 | Run 2 |
 | :--- | :--- | :---: | :---: |
| AIS-catcher Default @ 288K Windows | ```-s 288000``` | 590 | 636 |
| AIS-catcher Standard (non-coherent) @ 288K Windows|```-m 0 -s 288000``` |   455 | 429 |
| AIS-catcher Base (non-coherent) @ 288K Windows|```-m 1 -s 288000``` | 434 | 413 |
| AIS-catcher Default @ 1536K Ubuntu | |  748 | 708 |
| RTL-AIS @ 1600K Ubuntu |```-n``` | 521 | 428 |
| AISRec 2.003 (trial)  @ Windows | Sampling: Low, super fast| 557 | 569 |

For completeness I performed seperate runs with [AISRec](https://sites.google.com/site/feverlaysoft/home) and [RTL-AIS](https://github.com/dgiardini/rtl-ais) as well. AISRec has some excellent sensitivity and is one of the most user friendly packages out there. It is highly recommended. Unfortunately, and I believe it is again due to the USB ports on my laptop for Windows, I could not get it to run for newer versions which suggest that a higher sampling rate is used in the newer versions of AISrec. RTL-AIS  is a very efficient and elegant open source AIS receiver with minimal hardware requirements and is a pioneer in the field of open source AIS software. 

The first three rows are ran in parallel (i.e. on the same input signal) and therefore are comparable. The other runs are provided for information purposes and cannot be compared as message density fluctuates over time and have different system and hardware requirements. Nevertheless, these non-scientifically conducted experiments suggest that 1) the Default model can perform better than the Standard model and 2) a higher sampling rate should be preferred over a lower rate where possible. 

Same results for a different set up. Location: The Hague residential area with RTL-SDR v3 dongle and Shakespeare antenna with quite some blockage from surrounding buildings, we have the following message count (over 5 minute run).

| Model | 288K Windows | 1536K Ubuntu |
| :--- | :---: | :---: | 
| AIS-catcher Default | 101 | 175|
| AIS-catcher Standard (non-coherent) | 27 | 63| 
| AIS-catcher Base (non-coherent) | 21 | 54|

The results for each column are comparable as based on the same input signal. 

## Running multiple models

The command line provides  the ```-m``` option which allows for the selection of the specific receiver models (```AIS-catcher```has 4 tested models included and one so-called Challenger model - the potential next Default model).  Notice that you can execute multiple models in one run for benchmarking purposes but only the messages from the first model specified are displayed and forwarded. To benchmark different models specify ```-b``` for timing and/or ```-v``` to compare message count, e.g.:
```
AIS-catcher -s 1536000 -r posterholt_1536_2.raw -m 4 -m 2 -m 0 -m 1 -q -b -v
```
The program will run and summarize the performance (count and timing) of four decoding models:
```
[Challenger model (experimental)]	: 35 msgs at 25 msg/s
[AIS engine v0.06]			: 34 msgs at 24 msg/s
[Standard (non-coherent)]		: 3 msgs at 2.1 msg/s
[Base (non-coherent)]			: 2 msgs at 1.4 msg/s

[Challenger model (experimental)]	: 4.5e+02 ms
[AIS engine v0.06]			: 4e+02 ms
[Standard (non-coherent)]		: 2.1e+02 ms
[Base (non-coherent)]			: 1.9e+02 ms
```
In this example the default model performs quite well in contrast to the standard non-coherent engine with 34 messages identified versus 3 for the standard engine. This is typical when there are few messages with poor quality. However, it  doubles the decoding time and has a higher memory usage (800 floats) so needs more powerful hardware. Please note that the improvements seen for this particular file are an exception. 

## Releases

A release in binary format for Windows 32 bit (including required libraries) can be found for your convenience as part of the release section (AIS-catcher 0.06 W32.zip). Please note that you will have to install drivers using Zadig (https://www.rtl-sdr.com/tag/zadig/). After that, simply unpack the ZIP file in one directory and start the executable. For Linux systems, compilation instructions are below. 

## Compilation process for Linux/Raspberry Pi

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

## In progress

The current code has what I call a Challenger model that embeds some improvements to the default engine I am playing with. This can be activated on the command line with the switch ```-m 4```. At the time of writing the challenger model increases the range in which we update the phase estimate when the model is detecting training bits and then narrows it down again when a message has started. On my home station it gives a slight improvement after running for 5 hours 4 models in parallel:
```
[Challenger model (experimental)]	: 18335 msgs at 0.016 msg/s
[AIS engine v0.06]			: 17228 msgs at 0.015 msg/s
[Standard (non-coherent)]		: 5169 msgs at 0.009 msg/s
[Base (non-coherent)]			: 4659 msgs at 0.009 msg/s
```


## To do

- Documenting and finetuning the default decoding model
- Ongoing: further improvements to reception and testing (e.g. improve coherent demodulation, downsampling, etc)
- Performance improvements
- More testing: in particular in an area with high message density
- Access to hardware specific functionality, e.g. gain control
- Windows GUI
- Overflow detection
- Windows driver improvements
- Refining automatic frequency correction function
-
- ....
- ...

