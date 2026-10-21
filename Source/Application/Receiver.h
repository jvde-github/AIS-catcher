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
#include "JSONAIS.h"
#include "Common.h"
#include "Model.h"
#include "StreamCounter.h"
#include "AIS.h"
#include "JSON.h"
#include "DeviceManager.h"
#include "Logger.h"

class Receiver;

//--------------------------------------------
class OutputStatistics
{
public:
	std::vector<std::unique_ptr<IO::StreamCounter>> statistics;

	void connect(Receiver &r);
};

// Hardware + Model with output connectors for messages and JSON
class Receiver : public Setting
{

	bool timing = false;
	bool high_sensitivity = false;

	int sample_rate = 0, bandwidth = 0, ppm = 0, own_mmsi = -1;
	int receiver_index = -1;
	// a name for people, shown instead of the device's own in the viewer and the stats
	std::string description;
	FLOAT32 station_lat = LAT_UNDEFINED;
	FLOAT32 station_lon = LON_UNDEFINED;
	// the station it stands at, 0 (the default station) when the configuration
	// names none; whether its messages carry the number is the run's to say:
	// they do once stations are configured
	int station = 0;
	bool stamp_station = false;
	// the name of a station the configuration lists, empty for the default one it does not
	std::string station_name;

	//  Models
	std::vector<std::unique_ptr<AIS::Model>> models;

	AIS::Mode ChannelMode = AIS::Mode::AB;
	std::string ChannelNMEA = "AB";

	// Output
	std::vector<std::unique_ptr<AIS::JSONAIS>> jsonais;

	TAG tag;

public:
	Receiver() : Setting("Receiver") {}

	bool verbose = false;
	std::vector<std::string> zones;
	// extra zones per engine; the receiver's zones apply to all of them
	std::vector<std::vector<std::string>> engine_zones;

	// Device manager
	DeviceManager deviceManager;

public:
	DeviceManager& getDeviceManager() { return deviceManager; }

	// Settings
	const std::string &getDescription() const { return description; }

	int getStation() const { return station; }
	const std::string &getStationName() const { return station_name; }
	bool hasPosition() const { return isValidCoord(station_lat, station_lon); }
	FLOAT32 getLat() const { return station_lat; }
	FLOAT32 getLon() const { return station_lon; }
	// its station's name, where the station puts it, and whether its messages carry the station's number
	void placeAtStation(const std::string &name, FLOAT32 lat, FLOAT32 lon, bool stamp)
	{
		station_name = name;
		station_lat = lat;
		station_lon = lon;
		stamp_station = stamp;
	}

	Setting &SetKey(AIS::Keys key, const std::string &arg) override
	{
		switch (key)
		{
		case AIS::KEY_SETTING_VERBOSE:
			verbose = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_SENSITIVITY_HIGH:
			Warning() << "Receiver: \"sensitivity_high\" is deprecated and will be removed, use \"engines\": [ { \"type\": \"v1_high\" } ] instead";
			high_sensitivity = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_CHANNEL:
			setChannel(arg, "");
			break;
		case AIS::KEY_SETTING_NMEA_CHANNEL:
			setChannel(channelModeString(), arg);
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
		case AIS::KEY_SETTING_DESCRIPTION:
		case AIS::KEY_SETTING_DESC:
			description = arg;
			break;
		case AIS::KEY_SETTING_STATION:
			station = Util::Parse::Integer(arg, 0, 2147483647);
			break;
		default:
			throw std::runtime_error(getName() + ": unknown setting \"" + AIS::KeyMap[key][JSON_DICT_SETTING] + "\"");
		}
		return *this;
	}


	// Model
	void setChannel(std::string mode, std::string NMEA);

	// the mode as -c spells it, so a designation can re-apply it
	std::string channelModeString() const
	{
		switch (ChannelMode)
		{
		case AIS::Mode::CD:
			return "CD";
		case AIS::Mode::X:
			return "X";
		default:
			return "AB";
		}
	}
	void setTags(const std::string &s);
	void removeTags(const std::string &s);
	void clearTags() { tag.mode = 0; }

	bool &Timing() { return timing; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message> &Output(int i) { return models[i]->Output().out; }
	Connection<AIS::GPS> &OutputGPS(int i) { return models[i]->OutputGPS().out; }
	Connection<AIS::Control> &OutputControl(int i) { return models[i]->OutputControl().out; }
	Connection<Plane::ADSB> &OutputADSB(int i) { return models[i]->OutputADSB().out; }

	Connection<JSON::JSON> &OutputJSON(int i) { return jsonais[i]->out; }

	std::unique_ptr<AIS::Model> &addModel(int m);
	std::unique_ptr<AIS::Model> &Model(int i) { return models[i]; }
	int Count() { return models.size(); }
	// jsonais is only sized by setupModel(), so guard against being asked
	// before the models are wired
	uint64_t getGroupMask(int i) const { return i < (int)jsonais.size() ? jsonais[i]->out.getGroupOut() : 0; }
	bool inZone(int i, const std::string &zone) const
	{
		for (const auto &z : zones)
			if (z == zone)
				return true;
		if (i < (int)engine_zones.size())
			for (const auto &z : engine_zones[i])
				if (z == zone)
					return true;
		return false;
	}
	// whether a model output is in any zone, its receiver's or its engine's own
	bool hasZone(int i) const
	{
		return !zones.empty() || (i < (int)engine_zones.size() && !engine_zones[i].empty());
	}
	uint64_t getGroupMask() const {
		uint64_t mask = 0;
		for (int i = 0; i < (int)jsonais.size(); i++)
			mask |= jsonais[i]->out.getGroupOut();
		return mask;
	}

	void setupDevice();
	void setupModel(int &g, int idx);

	void play();
	void stop();
	void logSummary();
};

// Zone-based filtering: the group mask of every model output that overlaps one of
// the zones. An empty mask means the filter matches no receiver at all.
uint64_t resolveZones(const std::vector<std::unique_ptr<Receiver>> &receivers, const std::vector<std::string> &zones);
// The group mask of every model output in no zone at all; any_zoned says whether some are in one.
uint64_t resolveUnzoned(const std::vector<std::unique_ptr<Receiver>> &receivers, bool &any_zoned);
