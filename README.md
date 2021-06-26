# AIS-catcher - An AIS receiver for RTL-SDR dongles and Airspy HF+
This package will add the ```AIS-catcher``` command - a dual channel AIS receiver for RTL-SDR dongles and the Airspy HF+. The program provides the option to read and decode the raw discriminator output of a VHF receiver as well. Output is send in the form of NMEA messages to either screen or broadcasted over UDP. The idea behind ```AIS-catcher``` is that it should be easy to run various decoding model in parallel and read input from file to create an environment to test and benchmark different decoding models in a live environment.

## Purpose
AIS-catcher is created for research and educational purposes under the MIT license. DO NOT rely upon this software including for navigation and/or safety of life or property purposes.

AIS-catcher is developed to provide a platform that enables continuous improvements. Any suggestions, observation or sharing of recordings for setups where the current models are struggling is highly appreciated! The algorithm behind the default receiver model was self made by investigating signals and trying different ways to get a coherent model running and keeping it simple at the same time. So far I have not come across the approach, perhaps because memory and time consuming but undoubtly, in such a widely researched area, it might exist. Nevertheless it looks promising as a starting point for further developments. If I have some more free time I will try to expand. 

## Usage
````
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
	[-q surpress NMEA messages to screen (default: false)]https://github.com/dgiardini/rtl-ais
	[-p xx frequency correction for RTL SDR]
	[-u xx.xx.xx.xx yyy UDP address and port (default: off)]
	[-h display this message and terminate (default: false)]
	[-m xx run specific decoding model - 0: non-coherent, 1: base, 2: coherent (default: 2)]
	[	0: non-coherent, 1: base, 2: coherent, 3: FM discrimator output (default: 2)]
	[-b benchmark demodulation models - for development purposes (default: off)]
````

## Examples


To test a proper installation and/or compilation, we can first try to run the program on a RAW audio file as in this tutorial (https://github.com/freerange/ais-on-sdr/wiki/Testing-GNU-AIS):
```console
wget "https://github.com/freerange/ais-on-sdr/wiki/example-data/helsinki-210-messages.raw"
AIS-catcher  -m 3 -v -s 48000 -r cs16 helsinki-210-messages.raw
```
AIS-catcher on this file should extract roughly 361 AIVDM lines. Notice that if the sample rate is set at 48 KHz with switch ```-m 3```, AIS-catcher runs a decoding model that assumes the input is the output of an FM discriminator. In this case the program is similar to the following usage of GNUAIS:
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

The following commands recorded a signal with ```rtl_sdr``` at a sampling rate of 288K Hz and then subsequently decodes the input with AIS-catcher:
```console
rtl_sdr -s 288K -f 162M  test_288.raw
AIS-catcher -r test_288.raw -s 288000 -v 
```

## Considerations

AIS-catcher tunes in on a frequency of 162 MHz. However, due to deviations in the internal oscillator of RTL-SDR devices, the actual frequency can be slightly off which will result in no or poor reception of AIS signals. It is therefore important to provide the program with the necessary correction in parts-per-million (ppm) to offset this deviation where needed. For most of our testing we have used the RTL-SDR v3 dongle where in principle no frequency correction is needed as deviations are guaranteed to be small. For optimal reception though ensure you determine the necessary correction, e.g. using https://github.com/steve-m/kalibrate-rtl and provide as input via the ```-p``` switch on the command line.

On some laptops we observed that Windows was struggling with high volume of data transferred from the RTL SDR dongle to the PC. I am not sure why (likely some driver issue as Ubuntu on the same machine worked fine) but it is wortwhile to check if your system supports transferring from the dongle at a sampling rate of 1.536 MHz with the following command which is part of the osmocom rtl-sdr package:
```console
rtl_test -s 1536000
```
In case you observe a high number of lost data, the advice is to run AIS-catcher at a lower sampling rate for RTL SDR dongles:
```console
AIS-catcher -s 288000
```


## Running multiple models

The command line provides  the ```-m``` option which allows for the selection of specific receiver models (```AIS-catcher```has 4 models currently included).  Notice that you can execute multiple models in one run for benchmarking purposes but only the messages from the first model specified are displayed and forwarded. To benchmark different models specify ```-b``` for timing and/or ```-v``` to compare message count, e.g.:
```
AIS-catcher -s 1536000 -r posterholt_1536_2.raw -m 2 -m 0 -q -b -v
```
The program will run and summarize the performance (count and timing) of the two decoding models "coherent" and "standard". The output will look something like:
```
[AIS engine v0.06]		: 34 msgs at 45 msg/s
[Standard (non-coherent)]	: 3 msgs at 4 msg/s
[AIS engine v0.06]		: 4.1e+02 ms
[Standard (non-coherent)]	: 2.2e+02 ms
```
In this example the experimental coherent demodulation model performs quite well in contrast to the standard engine with 34 messages identified versus 3 for the standard engine. This is typical when there are few messages with poor quality. The coherent model is now the default but the improvements seen for this particular file are exceptional. 


## Available models

As highlighted in the previous section Currently 4 models are included in the program:

- `Default model`: a simple coherent demodulation model that tries to make local estimates of the phase offset. The idea was to find a balance between the reception quality of coherent models and robustness of non-coherent model. 
- `Base model (non-coherent)`: base model similar to rtl-ais with some modifications to PLL and filter [see https://jaspersnotebook.blogspot.com/2021/03/ais-vessel-tracking-designing.html].
- `Standard model (non-coherent)`: as the base model with more aggressive PLL
- `FM discriminator model`: as  the 'standard' model but assumes input is output of a FM discriminator, hence no FM demodulation takes place.

The default model is the most time and memory consuming but experiments suggest it to be the most effective. In my home station it improves message count by a factor 2 - 3. The reception quality of the `standard` model over the `base` model is more modest at the expense of roughly a 20% increase in computation time. Advice is to start with the default model, which should run fine on most modern hardware including a Raspberry 4B and then scale down to ```-m 0```or even ```m 1``` if needed.

To get a sense of the performance of the different models, I have run a simple test in two different setups whereby ```AIS-catcher``` ran the three models in parallel for 5 minutes. Due to the USB issues I have on my laptop for Windows (as described in a previous section), I have ran on Windows at a low sampling rate of 288K samples per second.

Location: Vlieland with NESDR RTL-SDR dongle with standard provided antenna included:
 | Model | Run 1 | Run 2 |
 | :---: | :---: | :---: |
| AIS-catcher Default @ 288K Windows | 590 | 636 |
| AIS-catcher Standard (non-coherent) @ 288K Windows| 455 | 429 |
| AIS-catcher Base (non-coherent) @ 288K Windows| 434 | 413 |
| AIS-catcher Default @ 1536K Ubuntu | 748 | 708 |
| RTL-AIS @ 1600K Ubuntu | 521 | 428 |
| AISRec 2.003 (trial) @ Low Windows | 557 | 569 |

For information, I performed seperate runs with AISRec and RTL-AIS as well. AISRec has some excellent sensitivity and is one of the most user friendly packages out there (https://sites.google.com/site/feverlaysoft/home). It is highly recommended. Unfortunately, and I believe it is again due to the USB ports on my laptop for Windows, I could not get it to run for newer versions which suggest that a higher sampling rate is used in the newer versions. RTL-AIS (https://github.com/dgiardini/rtl-ais) is a very efficient and elegant open source AIS receiver with minimal hardware requirements and is a pioneer for open source AIS software.

The first three rows are ran in parallel (i.e. on the same input signal) and therefore are comparable. The other runs are provided for information purposes and cannot be compared as message density fluctuates over time. Nevertheless, these non-scientific conducted experiments suggest that the default model can perform better than the standard model and a higher sampling rate should be preferred over a lower rate where possible.

Same results for a different set up. Location: The Hague residential area with RTL-SDR dongle and Shakespeare antenna with quite some blockage from surrounding buildings:

| Model | Run 1 | 
| :---: | :---: | 
| AIS-catcher Default @ 288K Windows | 101 | 
| AIS-catcher Standard (non-coherent) @ 288K Windows| 27 | 
| AIS-catcher Base (non-coherent) @ 288K Windows| 21 | 
| AIS-catcher Default @ 1536K Raspberry Pi 4B | 175 | 
| AIS-catcher Standard (non-coherent) @ 1536K Raspberry Pi 4B | 63 | 
| AIS-catcher Base (non-coherent) @ 1536K Raspberry Pi 4B | 54 | 
| RTL-AIS @ 1600K Ubuntu | 4 | 
| AISRec 2.003 (trial) @ Low Windows | 59 | 

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

## To do

- Ongoing: further improvements to reception and testing (e.g. improve coherent demodulation, downsampling, etc)
- Access to hardware specific functionality, e.g. gain control
- Windows GUI
- Overflow detection
- Windows driver improvements
- Refining automatic frequency correction function
- ....
- ...

