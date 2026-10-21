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

#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "Protocol.h"
#include "SiteConfig.h"
#include "StationRegistry.h"
#include "SystemFacts.h"
#include "Tracker.h"

// The station's state on an MQTT broker, for Home Assistant and anything
// else that reads retained topics: whether it is up (a last will says when it
// is not), the vessels and message rate, each station and receiver, the
// watched vessels' positions, and each event as it happens. With discovery on,
// Home Assistant finds the sensors by itself. Polled from the site's
// heartbeat; the connection never blocks it.
//
//   <topic>/availability        online | offline                    retained
//   <topic>/state               vessels, message rate, engine        retained
//   <topic>/station/<id>        online, last heard, vessels          retained
//   <topic>/receiver/<n>        state, messages a minute             retained
//   <topic>/vessel/<mmsi>       position of a watched vessel         retained
//   <topic>/event               each event of the journal
class MqttState
{
public:
	MqttState(Tracker &tracker, std::function<SystemFacts()> facts, std::function<StationRegistry()> stations)
		: tracker(tracker), facts(std::move(facts)), stations(std::move(stations)) {}
	~MqttState() { close(); }

	// a new broker or topic connects again; the same keeps the connection
	void configure(const MqttStateSpec &spec);
	// about once a second
	void poll(std::time_t now);

private:
	Tracker &tracker;
	std::function<SystemFacts()> facts;
	std::function<StationRegistry()> stations;
	MqttStateSpec spec;

	// the connection: TCP, TLS or a WebSocket below MQTT, as the URL says
	struct Link
	{
		Protocol::TCP tcp;
		Protocol::TLS tls;
		Protocol::WebSocket ws;
		Protocol::MQTT mqtt;
		Protocol::ProtocolBase *session = nullptr;
	};
	std::unique_ptr<Link> link;

	bool announced = false; // discovery and the first state went out on this connection
	std::time_t last_state = 0;
	uint64_t cursor = 0; // the journal's events published so far
	std::map<std::string, std::string> sent; // retained topics and what they hold, to send only a change

	void open();
	void close();
	std::string node() const;
	void retain(const std::string &topic, const std::string &payload, bool always = false);
	void announce();
	// the device every entity belongs to, and a watched vessel's tracker under its name once known
	void writeDevice(JSON::Writer &w);
	void announceVessel(uint32_t mmsi, const std::string &name, bool always = false);
	void publishState(std::time_t now, bool always);
	void publishEvents();
};
