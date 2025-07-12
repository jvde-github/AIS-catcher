# AIS-catcher: A multi-platform AIS Receiver

This repository presents the `AIS-catcher` software, a versatile dual-channel AIS receiver that is compatible with a wide range of Software Defined Radios (SDRs). These include RTL-SDR dongles (such as the ShipXplorer AIS dongle and RTL SDR Blog v4), AirSpy (Mini/R2/HF+), HackRF, SDRPlay, SoapySDR, and file/network input (ZMQ/RTL-TCP/SpyServer). AIS-catcher delivers output in the form of NMEA messages, which can be conveniently displayed on screen or forwarded via UDP/HTTP/TCP. Designed as a lightweight command line utility, AIS-catcher also incorporates a built-in web server for internal use within secure networks. The project home page including several realtime examples can be found at [aiscatcher.org](https://aiscatcher.org).

## Purpose

The purpose of `AIS-catcher` is to serve as a platform that encourages the perpetual enhancement of receiver models. We greatly value and appreciate any suggestions, observations, or shared recordings, particularly from setups where the existing models encounter difficulties.

## License

Copyright (C) 2021 - 2025 jvde.github at gmail.com. All rights reserved. Licensed under GNU General Public License v3.0.

## Important Disclaimer
`AIS-catcher` is created for research and educational purposes under the GNU GPL v3 license. It is a hobby project and has not been tested and designed for reliability and correctness. 
You can play with the software but it is the user's responsibility to use it prudently. So, DO NOT rely upon this software in any way including for navigation 
and/or safety of life or property purposes.
There are variations in the legislation concerning radio reception in the different administrations around the world. 
It is your responsibility to determine whether or not your local administration permits the reception and handling of AIS messages from ships. 
It is specifically forbidden to use this software for any illegal purpose whatsoever. 
Only use this software in regions where such use is permitted.

## Feature overview: Input -> Output

![image](https://github.com/user-attachments/assets/6677b833-bd2c-4338-babe-3817d6a7c3ea)

## The aiscatcher.org community

To join, ensure you're on the latest version, visit [aiscatcher.org](https://aiscatcher.org), and [add](https://aiscatcher.org/addstation) your station. Upon registration, you'll receive a personal sharing key. Simply run AIS-catcher on the command line with "-X" followed by your sharing key to share your station's raw AIS data with the community hub. This activates a "Community Feed" in your station's web viewer, accessible under map layers and some other features.


## Links

- Documentation: [here](https://docs.aiscatcher.org)
- Installation: [here](https://docs.aiscatcher.org/installation/overview)
- What is New? [here](https://docs.aiscatcher.org/what-is-new/)
- Forum: [here](https://github.com/jvde-github/AIS-catcher/discussions)
- Bug Reports: [here](https://github.com/jvde-github/AIS-catcher/issues)

