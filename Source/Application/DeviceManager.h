/*
    Copyright(c) 2021-2025 jvde.github@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "Common.h"

#include "Device/FileRAW.h"
#include "Device/FileWAV.h"
#include "Device/RTLSDR.h"
#include "Device/AIRSPYHF.h"
#include "Device/HACKRF.h"
#include "Device/RTLTCP.h"
#include "Device/AIRSPY.h"
#include "Device/SDRPLAY.h"
#include "Device/SpyServer.h"
#include "Device/SoapySDR.h"
#include "Device/Serial.h"
#include "Device/ZMQ.h"
#include "Device/UDP.h"
#include "Device/N2KsktCAN.h"
#include "Device/HYDRASDR.h"

class DeviceManager
{
    Type type = Type::NONE;
    std::string serial;

    // Device instances
    Device::RAWFile _RAW;
    Device::WAVFile _WAV;
    Device::RTLSDR _RTLSDR;
    Device::RTLTCP _RTLTCP;
    Device::SpyServer _SpyServer;
    Device::AIRSPYHF _AIRSPYHF;
    Device::AIRSPY _AIRSPY;
    Device::SDRPLAY _SDRPLAY;
    Device::HACKRF _HACKRF;
    Device::SOAPYSDR _SOAPYSDR;
    Device::SerialPort _SerialPort;
    Device::ZMQ _ZMQ;
    Device::UDP _UDP;
    Device::N2KSCAN _N2KSCAN;
    Device::HYDRASDR _HYDRASDR;

    Device::Device *getDeviceByType(Type type);

    // Available devices
    static std::vector<Device::Description> device_list;
    Device::Device *device = nullptr;

public:
    ~DeviceManager()
    {
        if (device)
            device->Close();
    }

    // Device accessors
    Device::RAWFile &RAW() { return _RAW; }
    Device::WAVFile &WAV() { return _WAV; }
    Device::RTLSDR &RTLSDR() { return _RTLSDR; }
    Device::RTLTCP &RTLTCP() { return _RTLTCP; }
    Device::SpyServer &SpyServer() { return _SpyServer; }
    Device::AIRSPYHF &AIRSPYHF() { return _AIRSPYHF; }
    Device::AIRSPY &AIRSPY() { return _AIRSPY; }
    Device::SDRPLAY &SDRPLAY() { return _SDRPLAY; }
    Device::HACKRF &HACKRF() { return _HACKRF; }
    Device::HYDRASDR &HYDRASDR() { return _HYDRASDR; }
    Device::SerialPort &SerialPort() { return _SerialPort; }
    Device::SOAPYSDR &SOAPYSDR() { return _SOAPYSDR; }
    Device::ZMQ &ZMQ() { return _ZMQ; }
    Device::UDP &UDP() { return _UDP; }
    Device::N2KSCAN &N2KSCAN() { return _N2KSCAN; }

    void refreshDevices();
    bool isTXTformatSet() { return getDeviceByType(type) ? (getDeviceByType(type)->getFormat() == Format::TXT) : false; }

    Type &InputType() { return type; }
    std::string &SerialNumber() { return serial; }

    Device::Device *getDevice() { return device; }

    bool openDevice(int sample_rate, int bandwidth, int ppm, int frequency, TAG &tag);
    void printAvailableDevices(bool JSON = false);
    void selectDeviceByIndex(int index);
};
