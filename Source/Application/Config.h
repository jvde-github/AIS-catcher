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

#include "AIS-catcher.h"

#include "Engine.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/Writer.h"

#include "File.h"

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
#include "Device/ZMQ.h"
#include "Device/HYDRASDR.h"

// What a configuration says wrong, without saying where: the reader adds
// "Config file: ", the -W switch names itself
struct ConfigError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

class Config
{
	Engine &_engine;
	// where the configuration file is: its relative file names are there
	static std::string base_dir;
	// managed mode (-E): the control panel's port and address, and the port beside it
	bool managed = false;
	int control_port = 0, viewer_port = 0;
	std::string bind;

	bool isActiveObject(const JSON::Value &m);
	void setModelfromJSON(const JSON::Member &m);
	void setEnginefromJSON(const JSON::Value &v);
	static int engineType(const std::string &s);

	// Create one output of type T per active object in the array, with settings applied
	template <typename T>
	void addOutputsFromJSON(const JSON::Member &m, const std::string &label, const char *default_timeout = nullptr)
	{
		if (!m.Get().isArray())
			throw std::runtime_error(label + " settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : m.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			_engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(new T()));
			IO::OutputMessage &o = *_engine.msg.back();
			if (default_timeout)
				o.SetKey(AIS::KEY_SETTING_TIMEOUT, default_timeout);
			setSettingsFromJSON(v, o);
		}
	}
	// the "db" array names its backend with a "type" field
	void addDatabaseOutputsFromJSON(const JSON::Member &m);

public:
	// one name-to-backend map for config file and -D alike; throws on unknown type
	static IO::OutputMessage *newDatabaseOutput(const std::string &type);

private:
	void setStationsFromJSON(const JSON::Value &v);
#ifdef HASAPI
	// a server's or a webhook's "filter": the viewer's filter terms
	static void viewFilterFromJSON(const JSON::Value &v, ViewFilter &filter);
	static void trackingFromJSON(const JSON::Value &v, SiteConfig &site);
	// `main_port` and `main_bind`: the server beside the control panel, for the entry marked "main"; none outside managed mode
	static void servicesFromJSON(const JSON::Value &v, SiteConfig &site, StationRegistry *registry, int main_port = 0, const std::string &main_bind = "");
	// the parts of a configuration a site reads as they stand: "tracking", "services", "webhooks", "mqtt_state"
	static void siteFromJSON(const JSON::JSON &root, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry, int main_port = 0, const std::string &main_bind = "");
	static void managedViewer(const JSON::Value &viewer, int port, const std::string &bind, SiteConfig &site, StationDefaults &defaults, StationRegistry *registry);
	// a server as a viewer section writes it (-N, "server", the old viewer section) or an entry of "services"
	struct ServerEntry;
	static bool serverKey(ServerEntry &e, int key, const std::string &value, const JSON::Value *json, StationDefaults *defaults);
	static void serverDone(ServerEntry &e, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry, const std::string &where);
	static void serversFromJSON(const JSON::Value &v, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry);
	// the -N being read: its settings gather until the next -N port or the end
	std::string pending_port;
	std::vector<std::pair<int, std::string>> viewer_args;
	void flushViewer();
#endif
#ifdef HASDELIVERY
	static void webhooksFromJSON(const JSON::Value &v, SiteConfig &site);
	static void mqttStateFromJSON(const JSON::Value &v, SiteConfig &site);
#endif
	void setReceiverfromJSON(const std::vector<JSON::Member> &m, bool unspecAllowed);
	void setReceiverFromArray(const JSON::Member &m);
	void setSharing(const std::vector<JSON::Member> &members);

public:
	Config(Engine &engine) : _engine(engine) {}

	// managed mode (-E): the site is read by its rules, its main server on viewer_port
	void setManaged(int control, int viewer, const std::string &address)
	{
		managed = true;
		control_port = control;
		viewer_port = viewer;
		bind = address;
	}
	void read(const std::string &file_config);
	void set(const std::string &str);
	// what the viewer sections and the receivers' stations come to once every
	// key is read, from the command line and the files it names
	void finish();
#ifdef HASAPI
	// -N [port] [key value]...: a web server; a port starts one, the keys that follow are its settings
	void viewerFromArgs(const std::string &port, const std::vector<std::pair<std::string, std::string>> &settings);
#endif
#ifdef HASDELIVERY
	// -W url [key value]...: a webhook for an http:// or https:// address, else the station's state on an MQTT
	// broker for Home Assistant, with the keys their sections in the file take
	void deliveryFromArgs(const std::string &url, const std::vector<std::pair<std::string, std::string>> &settings);
#endif

	// skip: one extra key the caller already consumed, e.g. the "type" selector
	static void setSettingsFromJSON(const JSON::Value &m, Setting &s, int skip = -1);
#ifdef HASAPI
	// managed mode's site: the main server beside the control panel, the other servers, the tracking and the
	// deliveries, and what the configuration says of the default station
	static void managedSite(const JSON::JSON &root, int control_port, int viewer_port, const std::string &bind, SiteConfig &site, StationDefaults &defaults, StationRegistry *registry);
#endif
	// the "stations" block of a configuration, into a registry
	static void stationsFromJSON(const JSON::Value &v, StationRegistry &into);
	// relative paths in a config file resolve against the file's directory
	static void setConfigBaseDir(const std::string &file_config);
};
