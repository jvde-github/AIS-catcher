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
#include <iostream>
#include <string.h>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "AIS-catcher.h"
#include "Signals.h"
#include "Common.h"
#include "Model.h"
#include "StreamCounter.h"
#include "Network.h"
#include "AIS.h"
#include "DB.h"
#include "History.h"
#include "JSON.h"
#include "DeviceManager.h"

class Receiver;

//--------------------------------------------
class OutputStatistics
{
public:
	std::vector<IO::StreamCounter> statistics;

	void connect(Receiver &r);
	void start();
};

// Hardware + Model with output connectors for messages and JSON
class Receiver : public Setting
{

	bool timing = false;

	int sample_rate = 0, bandwidth = 0, ppm = 0, own_mmsi = -1;
	int receiver_index = -1;
	FLOAT32 station_lat = LAT_UNDEFINED;
	FLOAT32 station_lon = LON_UNDEFINED;

	//  Models
	std::vector<std::unique_ptr<AIS::Model>> models;

	AIS::Mode ChannelMode = AIS::Mode::AB;
	std::string ChannelNMEA = "AB";

	// Output
	std::vector<AIS::JSONAIS> jsonais;

	TAG tag;

public:
	Receiver() : Setting("Receiver") {}

	bool verbose = false;
	std::vector<std::string> zones;

	// Device manager
	DeviceManager deviceManager;

public:
	DeviceManager& getDeviceManager() { return deviceManager; }

	// Settings
	Setting &SetKey(AIS::Keys key, const std::string &arg) override
	{
		switch (key)
		{
		case AIS::KEY_SETTING_VERBOSE:
			verbose = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_CHANNEL:
			setChannel(arg, "");
			break;
		case AIS::KEY_SETTING_META:
			setTags(arg);
			break;
		case AIS::KEY_SETTING_OWN_MMSI:
			own_mmsi = Util::Parse::Integer(arg);
			break;
		case AIS::KEY_SETTING_SERIAL:
			deviceManager.SerialNumber() = arg;
			break;
		case AIS::KEY_SETTING_INPUT:
			if (!Util::Parse::DeviceType(arg, deviceManager.InputType()))
				throw std::runtime_error("unknown input type: " + arg);
			break;
		case AIS::KEY_SETTING_SAMPLE_RATE:
			sample_rate = Util::Parse::Integer(arg, 12500, 12288000);
			break;
		case AIS::KEY_SETTING_BANDWIDTH:
			bandwidth = Util::Parse::Integer(arg, 0, 20000000);
			break;
		case AIS::KEY_SETTING_FREQOFFSET:
			ppm = Util::Parse::Integer(arg, -150, 150);
			break;
		case AIS::KEY_SETTING_LAT:
			station_lat = Util::Parse::Float(arg);
			break;
		case AIS::KEY_SETTING_LON:
			station_lon = Util::Parse::Float(arg);
			break;
		default:
			throw std::runtime_error(getName() + ": unknown setting \"" + AIS::KeyMap[key][JSON_DICT_SETTING] + "\"");
		}
		return *this;
	}

	const std::vector<AIS::Keys> &getAcceptedKeys() const override
	{
		static const std::vector<AIS::Keys> keys = {
			AIS::KEY_SETTING_VERBOSE, AIS::KEY_SETTING_CHANNEL,
			AIS::KEY_SETTING_META, AIS::KEY_SETTING_OWN_MMSI,
			AIS::KEY_SETTING_SERIAL, AIS::KEY_SETTING_INPUT,
			AIS::KEY_SETTING_SAMPLE_RATE, AIS::KEY_SETTING_BANDWIDTH,
			AIS::KEY_SETTING_FREQOFFSET, AIS::KEY_SETTING_LAT, AIS::KEY_SETTING_LON};
		return keys;
	}

	// Model
	void setChannel(std::string mode, std::string NMEA);
	void setTags(const std::string &s);
	void removeTags(const std::string &s);
	void clearTags() { tag.mode = 0; }

	bool &Timing() { return timing; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message> &Output(int i) { return models[i]->Output().out; }
	Connection<AIS::GPS> &OutputGPS(int i) { return models[i]->OutputGPS().out; }
	Connection<Plane::ADSB> &OutputADSB(int i) { return models[i]->OutputADSB().out; }

	Connection<JSON::JSON> &OutputJSON(int i) { return jsonais[i].out; }

	std::unique_ptr<AIS::Model> &addModel(int m);
	std::unique_ptr<AIS::Model> &Model(int i) { return models[i]; }
	int Count() { return models.size(); }
	uint32_t getGroupMask() const {
		uint32_t mask = 0;
		for (int i = 0; i < (int)jsonais.size(); i++)
			mask |= jsonais[i].out.getGroupOut();
		return mask;
	}

	void setup(int &g, int idx)
	{
		setupDevice();
		setupModel(g, idx);
	}
	void setupDevice();
	void setupModel(int &g, int idx);

	void play();
	void stop();
};
