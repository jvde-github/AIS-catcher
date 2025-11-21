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
class Receiver
{

	bool timing = false;

	int sample_rate = 0, bandwidth = 0, ppm = 0, own_mmsi = -1;
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
	bool verbose = false;

	// Device manager
	DeviceManager deviceManager;

public:
	DeviceManager& getDeviceManager() { return deviceManager; }

	// Model
	void setChannel(std::string mode) { setChannel(mode, ""); }
	void setChannel(std::string mode, std::string NMEA);
	void setOwnMMSI(int m) { own_mmsi = m; }
	void setTags(const std::string &s);
	void removeTags(const std::string &s);
	void clearTags() { tag.mode = 0; }
	void setLatLon(FLOAT32 lat, FLOAT32 lon)
	{
		station_lat = lat;
		station_lon = lon;
	}

	bool &Timing() { return timing; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message> &Output(int i) { return models[i]->Output().out; }
	Connection<AIS::GPS> &OutputGPS(int i) { return models[i]->OutputGPS().out; }
	Connection<Plane::ADSB> &OutputADSB(int i) { return models[i]->OutputADSB().out; }

	Connection<JSON::JSON> &OutputJSON(int i) { return jsonais[i].out; }

	void setSampleRate(int s) { sample_rate = s; }
	void setBandwidth(int b) { bandwidth = b; }
	void setPPM(int p) { ppm = p; }

	std::unique_ptr<AIS::Model> &addModel(int m);
	std::unique_ptr<AIS::Model> &Model(int i) { return models[i]; }
	int Count() { return models.size(); }

	void setup(int &g)
	{
		setupDevice();
		setupModel(g);
	}
	void setupDevice();
	void setupModel(int &g);

	void play();
	void stop();
};
