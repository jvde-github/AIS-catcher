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

#include "AIS-catcher.h"
#include "JSONAIS.h"
#include "Common.h"
#include "Model.h"
#include "StreamCounter.h"
#include "AIS.h"
#include "JSON.h"
#include "DeviceManager.h"
#include "Logger.h"

// Hardware + Model with output connectors for messages and JSON
class Receiver : public Setting
{

	bool timing_ = false;
	bool high_sensitivity = false;

	int sample_rate = 0, bandwidth = 0, ppm = 0, own_mmsi = -1;
	int receiver_index = -1;
	// a name for people, shown instead of the device's own on the status pages
	std::string description;
	FLOAT32 station_lat = LAT_UNDEFINED;
	FLOAT32 station_lon = LON_UNDEFINED;
	// the station it stands at, 0 (the default station) when the configuration
	// names none; whether its messages carry the number is the run's to say:
	// they do once stations are configured
	int station = 0;
	bool stamp_station = false;

	// the zones of the receiver, which every model is in
	std::vector<std::string> zones;

	AIS::Mode channel_mode = AIS::Mode::AB;
	std::string channel_nmea = "AB";

	TAG tag;

	// the bit a tag letter stands for: M (4), T (2), D (1); 0 for a letter that is none
	static int tagBit(char c);
	// a new model by its number, as -m counts them
	static AIS::Model *makeModel(int m);

public:
	// one model with what is its own: its number as addModel resolved it, the
	// zones of its own, its JSON output and the counter of its messages; the
	// output and the counter are made when the models are set up and wired
	struct Decoder
	{
		std::unique_ptr<AIS::Model> model;
		int id = -1;
		std::vector<std::string> zones;
		std::unique_ptr<AIS::JSONAIS> json;
		std::unique_ptr<IO::StreamCounter> counter;
	};

private:
	std::vector<Decoder> decoders;

public:
	Receiver() : Setting("Receiver") {}

	bool verbose = false;

	// Device manager
	DeviceManager deviceManager;

public:
	DeviceManager &devices() { return deviceManager; }
	// the device opened for it, null before setupDevice(); and the kind of input it is configured for
	Device::Device *device() const { return deviceManager.getDevice(); }
	Type inputType() const { return deviceManager.inputType(); }

	// Settings
	const std::string &getDescription() const { return description; }

	int getStation() const { return station; }
	bool hasPosition() const { return isValidCoord(station_lat, station_lon); }
	FLOAT32 getLat() const { return station_lat; }
	FLOAT32 getLon() const { return station_lon; }
	// where its station puts it, and whether its messages carry the station's number
	void placeAtStation(FLOAT32 lat, FLOAT32 lon, bool stamp)
	{
		station_lat = lat;
		station_lon = lon;
		stamp_station = stamp;
	}
	// the vessel this receiver is on, whose messages the models mark as its own
	void setOwnMMSI(int mmsi) { own_mmsi = mmsi; }

	Setting &applyKey(AIS::Keys key, const std::string &arg) override;

	// Model
	void setChannel(std::string mode, std::string NMEA);

	// the mode as -c spells it, so a designation can re-apply it
	const std::string &channelNMEA() const { return channel_nmea; }
	std::string channelModeString() const
	{
		switch (channel_mode)
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
	// the tags as -M takes them: D, T and M, those that are on
	std::string tagString() const;

	// whether its models time their decoding, for the totals at the end
	bool timing() const { return timing_; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message> &Output(int i) { return decoders[i].model->Output().out; }
	Connection<AIS::GPS> &OutputGPS(int i) { return decoders[i].model->OutputGPS().out; }
	Connection<AIS::Control> &OutputControl(int i) { return decoders[i].model->OutputControl().out; }
	Connection<Plane::ADSB> &OutputADSB(int i) { return decoders[i].model->OutputADSB().out; }

	Connection<JSON::JSON> &OutputJSON(int i) { return decoders[i].json->out; }

	// the model the input decodes with when none is named: by its format, else by the sensitivity
	int defaultModel();
	// a model by its number (-1 for the default), with the zones of its own
	AIS::Model &addModel(int m, const std::vector<std::string> &zones = std::vector<std::string>());
	AIS::Model &model(int i) { return *decoders[i].model; }
	int modelCount() const { return decoders.size(); }
	int modelId(int i) const { return decoders[i].id; }
	// a model's place among all: "#receiver-model"
	std::string modelIndex(int i) const { return "#" + std::to_string(receiver_index) + "-" + std::to_string(i); }
	// a model's name with its place, as the rates and totals say it
	std::string modelLabel(int i) const { return decoders[i].model->getName() + " " + modelIndex(i); }
	// a zone of the receiver, which every model is in
	void addZone(const std::string &zone) { zones.push_back(zone); }
	// the group of a model's outputs, once setupModel() assigned them
	uint64_t getGroupMask(int i) const { return decoders[i].json->out.getGroupOut(); }
	bool inZone(int i, const std::string &zone) const
	{
		for (const auto &z : zones)
			if (z == zone)
				return true;
		for (const auto &z : decoders[i].zones)
			if (z == zone)
				return true;
		return false;
	}
	// whether a model output is in any zone, its receiver's or its engine's own
	bool hasZone(int i) const
	{
		return !zones.empty() || !decoders[i].zones.empty();
	}
	uint64_t getGroupMask() const {
		uint64_t mask = 0;
		for (const auto &d : decoders)
			mask |= d.json->out.getGroupOut();
		return mask;
	}
	// the counters sit behind the models' message and ADS-B outputs, from here on
	void connectCounters();
	IO::StreamCounter &counter(int i) { return *decoders[i].counter; }
	// the messages all its models decoded, once the counters are connected
	uint64_t messages();

	void setupDevice();
	void setupModel(int &g, int idx);

	void play();
	void stop();
	void logSummary() const;
};

// what a receiver is called on the status pages: its own name, else its kind of device with its
// place among the receivers of that kind ("RTL-SDR #1", "Serial #2"), as the control panel names
// it; the console is "Console"
std::string receiverLabel(const std::vector<std::unique_ptr<Receiver>> &receivers, std::size_t k);
// whether any model output is in a zone, its receiver's or its engine's own
bool anyZoned(const std::vector<std::unique_ptr<Receiver>> &receivers);
// Zones are routes. An output with zones takes the receivers that share one; once any receiver
// is in a zone, an output without zones takes only the receivers without one. Narrowing only:
// what an output already keeps out, as the community feed keeps out network inputs, stays out.
// The group mask that is left of groups_in; a warning names `what` when nothing is.
uint64_t routeGroups(const std::vector<std::unique_ptr<Receiver>> &receivers, const std::vector<std::string> &zones, uint64_t groups_in, const std::string &what);
