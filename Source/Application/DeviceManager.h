/*
    Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include <memory>
#include <mutex>
#include <vector>

#include "Common.h"
#include "Device/Device.h"

class DeviceManager
{
    Type type = Type::NONE;
    std::string serial_;

    // One row per device type: its "-g<flag>" command line switch (0 when it has
    // none), its config file key, its Type, whether refreshDevices() asks it to
    // enumerate hardware, whether it is a radio, its name on the status pages
    // and how its device object is made. Network/file devices must not
    // enumerate: they push an unconditional pseudo-entry that would pollute the
    // device list. Rows are ordered enumerating-first; that order fixes the
    // "-d:x" indices.
    struct DeviceEntry
    {
        char flag;
        AIS::Keys key;
        Type type;
        bool enumerate;
        bool radio;
        const char *label;
        Device::Device *(*make)();
    };
    static const int ROWS = 15;
    static const DeviceEntry device_settings[];
    // the row of a type, a config file key or a -g letter; nullptr for none
    static const DeviceEntry *row(Type type);
    static const DeviceEntry *row(AIS::Keys key);
    static const DeviceEntry *row(char flag);

    // the device object of each row, made the first time a setting, the
    // enumeration or the input asks for it: a receiver pays for the kinds it uses
    std::unique_ptr<Device::Device> objects[ROWS];
    Device::Device &object(const DeviceEntry &d);

    Device::Device *getDeviceByType(Type type);
    static void printLocked();

    static std::vector<Device::Description> device_list;
    static std::mutex list_mtx;
    Device::Device *device = nullptr;

public:
    ~DeviceManager()
    {
        if (device)
            device->Close();
    }

    // The device Setting a "-g<flag>" switch or a config file key names,
    // nullptr when it names no device.
    Setting *settingForFlag(char flag);
    Setting *settingForKey(AIS::Keys key);
    // a device type's block in the configuration file, by its -g letter or its type: KEY_COUNT for none
    static AIS::Keys keyForFlag(char flag);
    static AIS::Keys keyForType(Type type);
    // a device type's block, by its key
    static bool isDeviceKey(AIS::Keys key);
    // An SDR radio: the kind selected when nothing is, and the kind whose
    // messages the community feed takes. Serial/N2K ports (e.g.
    // /dev/cu.debug-console) still enumerate but must be selected explicitly.
    static bool isRadio(Type type);
    // the kind of device as the control panel writes it; "Input" for a kind without a row
    static const char *label(Type type);

    void refreshDevices();
    Format getInputFormat()
    {
        Device::Device *d = getDeviceByType(type);
        return d ? d->getFormat() : Format::CU8;
    }

    // the kind of input and the serial number the configuration selects, when it names them
    Type inputType() const { return type; }
    void setInputType(Type t) { type = t; }
    const std::string &serial() const { return serial_; }
    void setSerial(const std::string &s) { serial_ = s; }

    // the device opened for the receiver, null before openDevice()
    Device::Device *getDevice() const { return device; }

    bool openDevice(int sample_rate, int bandwidth, int ppm, int frequency, TAG &tag);
    static void printAvailableDevices(bool JSON = false);
    static std::string getDeviceListJSON();
    void selectDeviceByIndex(int index);
};
