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

#include "MqttState.h"
#include "AIS-catcher.h"
#include "JournalJSON.h"
#include "Logger.h"
#include "Parse.h"
#include "Writer.h"

namespace
{
	// how often the state goes out even when nothing changed, and how long a
	// station may be quiet and still count as online
	const int REFRESH_S = 60, ONLINE_S = 300, STATE_S = 10;

	std::string json(const std::function<void(JSON::Writer &)> &f)
	{
		std::string out;
		JSON::Writer w(out);
		f(w);
		w.finish();
		return out;
	}
}

void MqttState::configure(const MqttStateSpec &next)
{
	if (next == spec && link)
		return;
	close();
	spec = next;
	if (spec.any())
		open();
}

void MqttState::open()
{
	std::string prot, host, port, path, username, password;
	Util::Parse::URL(spec.url, prot, username, password, host, port, path);
	PROTOCOL protocol = PROTOCOL::MQTT;
	if (!prot.empty() && !Util::Parse::Protocol(prot, protocol))
	{
		Error() << "MQTT state: unknown protocol in " << spec.url;
		return;
	}

	link.reset(new Link());
	Link &l = *link;
	switch (protocol)
	{
	case PROTOCOL::MQTTS:
		l.session = l.tcp.add(&l.tls);
		l.session = l.tls.add(&l.mqtt);
		break;
	case PROTOCOL::WSMQTT:
		l.session = l.tcp.add(&l.ws);
		l.session = l.ws.add(&l.mqtt);
		break;
	case PROTOCOL::WSSMQTT:
		l.session = l.tcp.add(&l.tls);
		l.session = l.tls.add(&l.ws);
		l.session = l.ws.add(&l.mqtt);
		break;
	case PROTOCOL::MQTT:
		l.session = l.tcp.add(&l.mqtt);
		break;
	default:
		Error() << "MQTT state: " << prot << " is not an MQTT protocol; use mqtt, mqtts, ws or wss.";
		link.reset();
		return;
	}
	if (protocol == PROTOCOL::WSMQTT || protocol == PROTOCOL::WSSMQTT)
	{
		l.ws.setPath(path);
		l.ws.setOptionKey(AIS::KEY_SETTING_PROTOCOLS, "mqtt");
		l.ws.setOptionKey(AIS::KEY_SETTING_BINARY, "on");
	}
	l.tcp.setOptionKey(AIS::KEY_SETTING_HOST, host);
	l.tcp.setOptionKey(AIS::KEY_SETTING_PORT, port.empty() ? (protocol == PROTOCOL::MQTTS ? "8883" : "1883") : port);
	const std::string user = spec.username.empty() ? username : spec.username;
	const std::string pass = spec.password.empty() ? password : spec.password;
	if (!user.empty())
		l.mqtt.setOptionKey(AIS::KEY_SETTING_USERNAME, user);
	if (!pass.empty())
		l.mqtt.setOptionKey(AIS::KEY_SETTING_PASSWORD, pass);
	l.mqtt.setOptionKey(AIS::KEY_SETTING_CLIENT_ID, spec.client_id.empty() ? node() : spec.client_id);
	l.mqtt.setWill(spec.topic + "/availability", "offline");

	announced = false;
	sent.clear();
	cursor = tracker.journal().newest();
	l.session->connect();
	Info() << "MQTT state: " << host << ":" << l.tcp.getPort() << ", topic " << spec.topic << (spec.discovery ? ", Home Assistant discovery" : "");
}

// says goodbye itself: the will is for a connection that dies
void MqttState::close()
{
	if (!link)
		return;
	if (link->mqtt.isConnected())
		link->mqtt.sendRetained("offline", spec.topic + "/availability");
	link->session->disconnect();
	link.reset();
	announced = false;
}

// the name Home Assistant knows this installation by: the topic, as an id
std::string MqttState::node() const
{
	std::string n;
	for (char c : spec.topic)
		n += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
	return n;
}

void MqttState::retain(const std::string &topic, const std::string &payload, bool always)
{
	auto it = sent.find(topic);
	if (!always && it != sent.end() && it->second == payload)
		return;
	if (link->mqtt.sendRetained(payload, topic) > 0)
		sent[topic] = payload;
}

void MqttState::poll(std::time_t now)
{
	if (!link)
		return;
	link->session->poll();
	if (!link->mqtt.isConnected())
	{
		if (announced)
			Warning() << "MQTT state: connection lost, trying again";
		announced = false;
		sent.clear();
		return;
	}
	if (!announced)
	{
		announced = true;
		retain(spec.topic + "/availability", "online", true);
		if (spec.discovery)
			announce();
		publishState(now, true);
		last_state = now;
		return;
	}
	if (now - last_state >= STATE_S)
	{
		publishState(now, now - last_state >= REFRESH_S);
		last_state = now;
	}
	publishEvents();
}

// Home Assistant's discovery: one retained config per entity, under the
// device this installation is, all available while the will says online
void MqttState::writeDevice(JSON::Writer &w)
{
	const std::string &t = spec.topic;
	// Home Assistant puts the device's name before each entity's: plain on the default topic,
	// the topic added when several installations share a broker
	const std::string name = t == MqttStateSpec().topic ? "AIS-catcher" : "AIS-catcher " + t;
	w.key("device").beginObject().key("identifiers").beginArray().val(node()).endArray()
		.kv("name", name).kv("manufacturer", "AIS-catcher").kv("model", "AIS receiver").kv("sw_version", VERSION).endObject()
		.kv("availability_topic", t + "/availability").kv("origin_name", "AIS-catcher");
}

// a watched vessel as a tracker: named by its MMSI until its name is heard
void MqttState::announceVessel(uint32_t mmsi, const std::string &name, bool always)
{
	const std::string n = node(), id = std::to_string(mmsi);
	retain("homeassistant/device_tracker/" + n + "/vessel_" + id + "/config", json([&](JSON::Writer &w) {
			   w.beginObject().kv("name", "Vessel " + (name.empty() ? id : name)).kv("unique_id", n + "_vessel_" + id)
				   .kv("json_attributes_topic", spec.topic + "/vessel/" + id).kv("source_type", "gps").kv("icon", "mdi:ferry");
			   writeDevice(w);
			   w.endObject();
		   }),
		   always);
}

void MqttState::announce()
{
	const std::string n = node(), t = spec.topic, prefix = "homeassistant/";
	const auto device = [&](JSON::Writer &w) { writeDevice(w); };
	const auto entity = [&](const std::string &component, const std::string &id, const std::string &name, const std::string &state,
							const std::string &value, const std::function<void(JSON::Writer &)> &more) {
		retain(prefix + component + "/" + n + "/" + id + "/config", json([&](JSON::Writer &w) {
				   w.beginObject().kv("name", name).kv("unique_id", n + "_" + id).kv("state_topic", state);
				   if (!value.empty())
					   w.kv("value_template", value);
				   if (more)
					   more(w);
				   device(w);
				   w.endObject();
			   }),
			   true);
	};

	entity("sensor", "vessels", "Vessels", t + "/state", "{{ value_json.vessels }}", [](JSON::Writer &w) {
		w.kv("unit_of_measurement", "vessels").kv("state_class", "measurement").kv("icon", "mdi:ferry");
	});
	entity("sensor", "message_rate", "Message rate", t + "/state", "{{ value_json.messages_per_minute }}", [](JSON::Writer &w) {
		w.kv("unit_of_measurement", "msg/min").kv("state_class", "measurement").kv("icon", "mdi:radio-tower");
	});
	entity("binary_sensor", "engine", "Receiving", t + "/state", "{{ 'ON' if value_json.engine_running else 'OFF' }}",
		   [](JSON::Writer &w) { w.kv("device_class", "running"); });
	entity("event", "events", "Events", t + "/event", "", [](JSON::Writer &w) {
		w.key("event_types").beginArray();
		for (const auto &k : Tracking::kindNames())
			if (k.journal)
				w.val(k.journal);
		w.endArray().kv("icon", "mdi:bell-ring");
	});

	stations().forEach([&](const StationRegistry::Station &s) {
		const std::string id = std::to_string(s.id), topic = t + "/station/" + id;
		// "Station" in front, so a station named after its receiver reads apart from the receiver
		const std::string label = s.name.empty() ? "Station " + id : s.name.compare(0, 8, "Station ") == 0 ? s.name : "Station " + s.name;
		entity("binary_sensor", "station_" + id, label + " online", topic, "{{ 'ON' if value_json.online else 'OFF' }}",
			   [&](JSON::Writer &w) { w.kv("device_class", "connectivity").kv("json_attributes_topic", topic); });
		entity("sensor", "station_" + id + "_vessels", label + " vessels", topic, "{{ value_json.vessels }}",
			   [](JSON::Writer &w) { w.kv("unit_of_measurement", "vessels").kv("state_class", "measurement").kv("icon", "mdi:ferry"); });
	});
	for (const auto &r : facts().receivers)
	{
		const std::string id = std::to_string(r.index), topic = t + "/receiver/" + id;
		entity("binary_sensor", "receiver_" + id, "Receiver " + id + " streaming", topic, "{{ 'ON' if value_json.state == 'streaming' else 'OFF' }}",
			   [&](JSON::Writer &w) { w.kv("device_class", "running").kv("json_attributes_topic", topic); });
	}
	for (uint32_t mmsi : spec.watch)
	{
		std::string name;
		tracker.ships().withShip(mmsi, [&](const Ship &s, int) { name = s.shipname; });
		announceVessel(mmsi, name, true);
	}
}

// the vessels in view, per station, the receivers and the watched vessels:
// only what changed, unless `always`
void MqttState::publishState(std::time_t now, bool always)
{
	const SystemFacts f = facts();
	DB &db = tracker.ships();
	const std::time_t window = now - db.timeHistory(), recent = now - Ship::STATION_WINDOW;
	int vessels = 0;
	std::map<int, int> heard; // per station, the vessels it heard within the station window
	db.forEachRecent(now, window < recent ? window : recent, [&](int, const Ship &s, long) {
		if (s.last_signal >= window)
			vessels++;
		s.forEachHeard([&](int station, std::time_t t) {
			if (t >= recent)
				heard[station]++;
		});
	});
	uint64_t per_minute = 0;
	for (const auto &r : f.receivers)
		per_minute += r.per_minute;

	const std::string t = spec.topic;
	retain(t + "/state", json([&](JSON::Writer &w) {
			   w.beginObject().kv("vessels", vessels).kv("messages_per_minute", (unsigned long long)per_minute)
				   .kv("engine_running", f.running).kv("receivers", (long long)f.receivers.size()).endObject();
		   }),
		   always);
	stations().forEach([&](const StationRegistry::Station &s) {
		const std::time_t last = tracker.lastHeard(s.id);
		retain(t + "/station/" + std::to_string(s.id), json([&](JSON::Writer &w) {
				   w.beginObject().kv("name", s.name).kv("online", last > 0 && now - last < ONLINE_S).kv("vessels", heard[s.id]);
				   if (last)
					   w.kv("last_heard", Services::isoTime(last));
				   w.endObject();
			   }),
			   always);
	});
	for (const auto &r : f.receivers)
		retain(t + "/receiver/" + std::to_string(r.index), json([&](JSON::Writer &w) {
				   w.beginObject().kv("state", r.status()).kv("station", r.station)
					   .kv("input", r.input).kv("messages_per_minute", (unsigned long long)r.per_minute).endObject();
			   }),
			   always);
	for (uint32_t mmsi : spec.watch)
	{
		std::string payload, name;
		db.withShip(mmsi, [&](const Ship &s, int) {
			name = s.shipname;
			payload = json([&](JSON::Writer &w) {
				w.beginObject().kv("mmsi", (long long)mmsi).kv("name", std::string(s.shipname))
					.kv("in_range", s.last_signal >= window).kv("last_seen", Services::isoTime(s.last_signal));
				if (isValidCoord(s.lat, s.lon))
					w.kv("latitude", s.lat).kv("longitude", s.lon).kv("gps_accuracy", 10);
				if (s.speed != SPEED_UNDEFINED)
					w.kv("speed_kn", s.speed);
				w.endObject();
			});
		});
		if (!payload.empty())
			retain(t + "/vessel/" + std::to_string(mmsi), payload, always);
		// the tracker takes the vessel's name once it is heard
		if (!name.empty())
			announceVessel(mmsi, name);
	}
}

// each new event once, with the event_type Home Assistant's event entity reads
void MqttState::publishEvents()
{
	std::vector<Journal::Entry> entries;
	if (!tracker.journal().since(cursor, 100, entries))
	{
		cursor = tracker.journal().newest();
		return;
	}
	const std::string topic = spec.topic + "/event";
	for (const auto &e : entries)
	{
		const std::string body = json([&](JSON::Writer &w) {
			Services::writeEvent(w, e, [&](JSON::Writer &lead) { lead.kv("event_type", Tracking::journalName(e.event.kind)); });
		});
		if (link->mqtt.send(body.data(), (int)body.size(), topic) <= 0)
			return; // the rest when the connection is back
		cursor = e.seq;
	}
}
