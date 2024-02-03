# Installation

This page covers build and installation instructions for AIS-catcher across different platforms.

## Prerequisites

Ensure you have the required dependencies installed:

**Linux**

```bash
sudo apt install git cmake gcc g++ pkg-config
```

**MacOS** 

```bash 
brew install cmake
```

**Windows**

- Visual Studio
- vcpkg

## Getting Sources

Clone the repository:

```bash
git clone https://github.com/jvde-github/AIS-catcher.git
```

## Building

**Linux/MacOS**

```bash
cd AIS-catcher
mkdir build
cd build
cmake ..
make
sudo make install
```

**Windows**

- Use Visual Studio solution file in `msvc` folder
- Or build with vcpkg

## SDR Drivers

Install libraries for your SDR hardware:

| Platform | RTLSDR | Airspy | SDRPlay | HackRF |
|-|-|-|-|-|
| Linux | `librtlsdr-dev` | `libairspy-dev` | Download SDK | `libhackrf-dev` |   
| MacOS | `librtlsdr` | `airspy` | N/A | `hackrf` |
| Windows | `rtlsdr` | N/A | Download SDK | N/A |

## Troubleshooting

- Enable debug logs with `-v` option 
- Check required libraries are installed
- Post issues on GitHub

The [usage guide](usage.md) covers running AIS-catcher with different SDRs and options.
