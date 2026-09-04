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
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

#include "Common.h"
#include "AIS.h"
#include "Keys.h"
#include "JSON.h"
#include "Writer.h"
#include "SlotTable.h"

// Store for binary (application-specific) messages: the DAC/FI payloads of
// types 6 and 8, kept newest-first as one item per identity with a count.

class BinaryStore
{
public:
	struct Item
	{
		enum Kind : uint8_t { TEXT, PERSONS, METEO, ATON, SIGNAL, AREA, LOCK, SAFETY };

		Kind kind = TEXT;
		int type = 0, dac = -1, fi = -1, sub = 0;
		uint32_t mmsi = 0, anchor = 0;
		// identity scope, not the transmitter (that is mmsi): zero for sensor kinds so copies via different towers merge
		uint32_t sender = 0;
		uint64_t hash = 0;
		// identity on the wire and in the table: 53 bits so JavaScript keeps it exact
		uint64_t key = 0;
		uint64_t seq = 0;
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		uint32_t count = 0;
		time_t first = 0, last = 0;
		std::string json;
		std::string label;  // station or lock id, the pill text
		std::string shapes; // area notice geometry
		int atype = -1;
		int marker = -1;    // marker slot when the item stands on its own position
		int next = -1;      // next member of that marker
		uint32_t owner = 0;   // the ship this item badges on when it has no marker
		int onext = -1;       // next item of that ship
		uint32_t sent_by = 0; // the transmitter whose sent chain holds it
		int snext = -1;       // next item that transmitter sent
	};

	// what a ship shows: the items about it (addressed to it, or its own
	// reports) and, on a second chain, everything it transmitted
	struct OwnerChain
	{
		int head = -1;
		int sent = -1;
	};

	// A marker is what the map draws: the items of one identity at one spot,
	// merged across senders and message types, aggregated from its members.
	struct Marker
	{
		enum Flags : uint8_t { NEW = 1, MOVED = 2, UPDATED = 4 };

		uint64_t key = 0;
		Item::Kind kind = Item::TEXT;
		uint8_t flags = 0;
		bool fresh = true;
		std::string label, shapes;
		int atype = -1;
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		uint32_t count = 0;
		time_t first = 0, last = 0;
		uint64_t seq = 0;
		int head = -1;
		std::vector<std::pair<uint32_t, uint32_t>> senders;
	};

	// the viewer fades binary items past 15 minutes; this removes them entirely
	int ttl = 45 * 60;
	// kinds that report on their own transmitter rather than address a party
	static bool aboutSender(Item::Kind k) { return k != Item::TEXT && k != Item::SAFETY && k != Item::LOCK && k != Item::AREA && k != Item::SIGNAL; }
	// a located item this close to its sender badges on the sender instead
	static constexpr float SNAP_DEG = 0.001f;
	// the most one ship's message list returns
	enum { MAX_PER_SHIP = 200 };

	void setup(int capacity)
	{
		items.setup(capacity, 2 * capacity + 1);
		markers.setup(capacity, 2 * capacity + 1);
		// an item can put two ships on record: its owner and its transmitter
		owners.setup(2 * capacity, 4 * capacity + 1);
	}
	// returns the item's slot, or -1 when the message made no item
	int process(const JSON::JSON &data, FLOAT32 sender_lat, FLOAT32 sender_lon);

	// An item without a marker badges on the ship it is about: the addressee
	// of a text or lock message when that ship is known, else the sender. A
	// status report (AtoN monitoring, persons on board, a sensor reading) is
	// about its transmitter even when it is addressed to a monitoring station,
	// so it stays on the sender. `known` answers whether an MMSI is in the table.
	template <typename Known>
	void settle(int h, Known known)
	{
		Item &b = items[h];
		uint32_t owner = 0;
		if (b.marker < 0)
			owner = (b.anchor && !aboutSender(b.kind) && known(b.anchor)) ? b.anchor : b.mmsi;
		if (b.owner == owner)
			return;
		unlinkFromOwner(h);
		if (owner)
			linkToOwner(h, owner);
	}

	// the ship row's packed badge: count (4b) | newest kind (3b) | age bucket (2b),
	// over what is about the ship and what it sent, an item on both chains once
	uint16_t badge(uint32_t mmsi, std::time_t now) const
	{
		int c = owners.find(mmsi);
		if (c == owners.NIL)
			return 0;
		unsigned n = 0;
		int newest = -1;
		auto tally = [&](int p) {
			const Item &it = items[p];
			if ((long int)now - (long int)it.last > ttl)
				return;
			n++;
			if (newest < 0 || it.last > items[newest].last)
				newest = p;
		};
		for (int p = owners[c].head; p >= 0; p = items[p].onext)
			tally(p);
		for (int p = owners[c].sent; p >= 0; p = items[p].snext)
			if (items[p].owner != mmsi)
				tally(p);
		if (!n)
			return 0;
		long int age = (long int)now - (long int)items[newest].last;
		unsigned bucket = age > 1800 ? 2 : age > 900 ? 1 : 0;
		return (uint16_t)(MIN(n, 15u) | ((unsigned)items[newest].kind << 4) | (bucket << 7));
	}

	// ship-attached items (all, one ship's, or one marker's members)
	void writeJSON(JSON::Writer &w, std::time_t now, std::time_t since, uint64_t marker_key = 0, uint32_t owner_mmsi = 0) const;
	// markers changed after `since` (a change sequence, not a time), and the removals;
	// the caller refreshes first and writes the document around them
	void writeMarkerRows(JSON::Writer &w, uint64_t since) const;
	void writeRemoved(JSON::Writer &w, uint64_t since) const;
	// the change counter, shared with whatever else feeds the object feed
	uint64_t &sequence() { return next_seq; }
	// drops expired members, re-aggregates and sequences the markers that changed
	void refresh(std::time_t now);
	// every live marker, brought up to date first; f returns false to stop
	template <typename F>
	void forEachMarker(std::time_t now, F f)
	{
		refresh(now);
		markers.forEach([&](int mh) { return f(markers[mh]); });
	}

private:
	static uint64_t hashText(const std::string &s)
	{
		uint64_t h = 1469598103934665603ULL;
		for (unsigned char c : s)
			h = (h ^ c) * 1099511628211ULL;
		return h;
	}

	static uint64_t hashInt(uint64_t h, int32_t v)
	{
		for (int i = 0; i < 4; i++)
			h = (h ^ (uint8_t)(v >> (i * 8))) * 1099511628211ULL;
		return h;
	}

	static uint64_t hashPosition(uint64_t h, FLOAT32 lat, FLOAT32 lon)
	{
		return hashInt(hashInt(h, (int32_t)(lat * 10000 + (lat < 0 ? -0.5f : 0.5f))),
					   (int32_t)(lon * 10000 + (lon < 0 ? -0.5f : 0.5f)));
	}

	enum KeyKind { BK_SKIP, BK_META, BK_TEXT, BK_PERSONS, BK_VALUE, BK_ATON, BK_SIGNAL, BK_AREA, BK_LOCK };

	static KeyKind classify(int &key, float &scale)
	{
		scale = 1;
		switch (key)
		{
		case AIS::KEY_WIND_SPEED_AVG: key = AIS::KEY_WSPEED; return BK_VALUE;
		case AIS::KEY_WIND_GUST_SPEED: key = AIS::KEY_WGUST; return BK_VALUE;
		case AIS::KEY_WIND_DIRECTION_AVG: key = AIS::KEY_WDIR; return BK_VALUE;
		case AIS::KEY_AIR_TEMPERATURE: key = AIS::KEY_AIRTEMP; return BK_VALUE;
		case AIS::KEY_DEW_POINT: key = AIS::KEY_DEWPOINT; return BK_VALUE;
		case AIS::KEY_BAROMETRIC_PRESSURE: key = AIS::KEY_PRESSURE; return BK_VALUE;
		case AIS::KEY_WATER_TEMPERATURE: key = AIS::KEY_WATERTEMP; return BK_VALUE;
		case AIS::KEY_VISIBILITY_KM: key = AIS::KEY_VISIBILITY; scale = 1 / 1.852f; return BK_VALUE;
		case AIS::KEY_WSPEED: case AIS::KEY_WGUST: case AIS::KEY_WDIR: case AIS::KEY_WGUSTDIR:
		case AIS::KEY_AIRTEMP: case AIS::KEY_HUMIDITY: case AIS::KEY_DEWPOINT: case AIS::KEY_PRESSURE: case AIS::KEY_PRESSURETEND:
		case AIS::KEY_VISIBILITY: case AIS::KEY_WATERLEVEL: case AIS::KEY_LEVELTREND:
		case AIS::KEY_CSPEED: case AIS::KEY_CDIR: case AIS::KEY_CSPEED2: case AIS::KEY_CDIR2: case AIS::KEY_CDEPTH2:
		case AIS::KEY_CSPEED3: case AIS::KEY_CDIR3: case AIS::KEY_CDEPTH3:
		case AIS::KEY_WAVEHEIGHT: case AIS::KEY_WAVEPERIOD: case AIS::KEY_WAVEDIR:
		case AIS::KEY_SWELLHEIGHT: case AIS::KEY_SWELLPERIOD: case AIS::KEY_SWELLDIR:
		case AIS::KEY_SEASTATE: case AIS::KEY_WATERTEMP: case AIS::KEY_PRECIPTYPE: case AIS::KEY_SALINITY: case AIS::KEY_ICE: case AIS::KEY_WATER_FLOW:
			return BK_VALUE;
		case AIS::KEY_DAC: case AIS::KEY_FID: case AIS::KEY_MESSAGE_ID: case AIS::KEY_LAT: case AIS::KEY_LON:
		case AIS::KEY_STATION_ID: case AIS::KEY_DEST_MMSI: case AIS::KEY_ACK_REQUIRED: case AIS::KEY_LINKAGE_ID:
		case AIS::KEY_STATION_NAME: case AIS::KEY_NAME: case AIS::KEY_SITE_ID: case AIS::KEY_REPORT_TYPE:
		case AIS::KEY_UTC_DAY: case AIS::KEY_UTC_HOUR: case AIS::KEY_UTC_MINUTE:
		case AIS::KEY_UN_COUNTRY: case AIS::KEY_UN_LOCODE: case AIS::KEY_FAIRWAY_SECTION: case AIS::KEY_TERMINAL_CODE:
		case AIS::KEY_FAIRWAY_HECTOMETRE: case AIS::KEY_TUGBOATS: case AIS::KEY_AIR_DRAUGHT:
		case AIS::KEY_BERTH_TYPE: case AIS::KEY_BERTH_NUMBER: case AIS::KEY_BERTH_ARRIVAL_TIME:
		case AIS::KEY_BERTH_DEPARTURE_TIME: case AIS::KEY_BERTH_LAT: case AIS::KEY_BERTH_LON:
			return BK_META;
		case AIS::KEY_ASM_VOLTAGE_DATA: case AIS::KEY_ASM_CURRENT_DATA: case AIS::KEY_ASM_POWER_SUPPLY_TYPE:
		case AIS::KEY_ASM_LIGHT_STATUS: case AIS::KEY_ASM_BATTERY_STATUS: case AIS::KEY_ASM_OFF_POSITION_STATUS:
		case AIS::KEY_ANA_INT: case AIS::KEY_ANA_EXT1: case AIS::KEY_ANA_EXT2:
		case AIS::KEY_RACON: case AIS::KEY_HEALTH: case AIS::KEY_STAT_EXT: case AIS::KEY_OFF_POSITION:
			return BK_ATON;
		case AIS::KEY_TRAFFIC_SIGNAL: case AIS::KEY_NEXT_SIGNAL:
			return BK_SIGNAL;
		case AIS::KEY_AREA_NOTICE_TYPE: case AIS::KEY_AREA_NOTICE_NAME: case AIS::KEY_AREA_NOTICE_DURATION:
		case AIS::KEY_AREA_NOTICE_LAT: case AIS::KEY_AREA_NOTICE_LON: case AIS::KEY_AREA_SHAPES:
		case AIS::KEY_NE_LON: case AIS::KEY_NE_LAT: case AIS::KEY_SW_LON: case AIS::KEY_SW_LAT:
			return BK_AREA;
		// what a type 23 group assignment asks of the vessels in its rectangle
		case AIS::KEY_STATION_TYPE: case AIS::KEY_SHIPTYPE: case AIS::KEY_SHIPTYPE_TEXT: case AIS::KEY_TXRX: case AIS::KEY_INTERVAL: case AIS::KEY_QUIET:
			return BK_META;
		case AIS::KEY_TEXT:
			return BK_TEXT;
		case AIS::KEY_CREW_COUNT: case AIS::KEY_PASSENGER_COUNT: case AIS::KEY_SHIPBOARD_PERSONNEL_COUNT:
			return BK_PERSONS;
		case AIS::KEY_LOCK_ID: case AIS::KEY_LOCK_SCHEDULE: case AIS::KEY_VESSEL_NAME:
		case AIS::KEY_LAST_LOCATION: case AIS::KEY_LAST_ATA: case AIS::KEY_FIRST_LOCK: case AIS::KEY_FIRST_LOCK_ETA:
		case AIS::KEY_SECOND_LOCK: case AIS::KEY_SECOND_LOCK_ETA: case AIS::KEY_DELAY_LOCK:
		case AIS::KEY_RTA: case AIS::KEY_ETA: case AIS::KEY_LOCK_STATUS: case AIS::KEY_TIDAL: case AIS::KEY_BERTH_NAME:
			return BK_LOCK;
		default:
			return BK_SKIP;
		}
	}

	static uint64_t markerKey(const Item &it)
	{
		uint64_t h = hashText(it.label);
		h = hashInt(h, (int32_t)std::lround(it.lat * 1000));
		h = hashInt(h, (int32_t)std::lround(it.lon * 1000));
		h = (h ^ ((uint64_t)it.kind << 48)) & 0x1FFFFFFFFFFFFFULL;
		return h ? h : 1;
	}

	void unlinkFromMarker(int h)
	{
		Item &it = items[h];
		if (it.marker < 0)
			return;
		Marker &m = markers[it.marker];
		if (m.head == h)
			m.head = it.next;
		else
			for (int p = m.head; p >= 0; p = items[p].next)
				if (items[p].next == h)
				{
					items[p].next = it.next;
					break;
				}
		it.marker = it.next = -1;
	}

	void linkToMarker(int h, uint64_t key)
	{
		int m = markers.find(key);
		if (m == markers.NIL)
		{
			m = markers.create(key);
			Marker &old = markers[m];
			// a recycled marker: its members lose their home
			for (int p = old.head; p >= 0;)
			{
				int n = items[p].next;
				items[p].marker = items[p].next = -1;
				p = n;
			}
			old = Marker();
			old.key = key;
			old.kind = items[h].kind;
		}
		Item &it = items[h];
		it.marker = m;
		it.next = markers[m].head;
		markers[m].head = h;
	}

	void unlinkFromOwner(int h)
	{
		Item &it = items[h];
		if (!it.owner)
			return;
		int c = owners.find(it.owner);
		if (c != owners.NIL)
		{
			OwnerChain &oc = owners[c];
			if (oc.head == h)
				oc.head = it.onext;
			else
				for (int p = oc.head; p >= 0; p = items[p].onext)
					if (items[p].onext == h)
					{
						items[p].onext = it.onext;
						break;
					}
			if (oc.head < 0 && oc.sent < 0)
				owners.remove(it.owner);
		}
		it.owner = 0;
		it.onext = -1;
	}

	void unlinkFromSent(int h)
	{
		Item &it = items[h];
		if (!it.sent_by)
			return;
		int c = owners.find(it.sent_by);
		if (c != owners.NIL)
		{
			OwnerChain &oc = owners[c];
			if (oc.sent == h)
				oc.sent = it.snext;
			else
				for (int p = oc.sent; p >= 0; p = items[p].snext)
					if (items[p].snext == h)
					{
						items[p].snext = it.snext;
						break;
					}
			if (oc.head < 0 && oc.sent < 0)
				owners.remove(it.sent_by);
		}
		it.sent_by = 0;
		it.snext = -1;
	}

	// the record of a ship, made when it has none; a recycled record's items lose their ship
	int claimChains(uint32_t mmsi)
	{
		int c = owners.find(mmsi);
		if (c != owners.NIL)
			return c;
		c = owners.create(mmsi);
		OwnerChain &old = owners[c];
		for (int p = old.head; p >= 0;)
		{
			int n = items[p].onext;
			items[p].owner = 0;
			items[p].onext = -1;
			p = n;
		}
		for (int p = old.sent; p >= 0;)
		{
			int n = items[p].snext;
			items[p].sent_by = 0;
			items[p].snext = -1;
			p = n;
		}
		old.head = old.sent = -1;
		return c;
	}

	void linkToOwner(int h, uint32_t owner)
	{
		int c = claimChains(owner);
		Item &it = items[h];
		it.owner = owner;
		it.onext = owners[c].head;
		owners[c].head = h;
	}

	void linkToSent(int h, uint32_t mmsi)
	{
		int c = claimChains(mmsi);
		Item &it = items[h];
		it.sent_by = mmsi;
		it.snext = owners[c].sent;
		owners[c].sent = h;
	}

	void tombstone(uint64_t key, uint64_t seq)
	{
		tombstones.push_back(std::make_pair(key, seq));
		if (tombstones.size() > 256)
			tombstones.erase(tombstones.begin(), tombstones.begin() + 64);
	}

	SlotTable<Item, uint64_t> items;
	SlotTable<Marker, uint64_t> markers;
	SlotTable<OwnerChain, uint32_t> owners;
	std::vector<std::pair<uint64_t, uint64_t>> tombstones;
	uint64_t next_seq = 0;
};

inline int BinaryStore::process(const JSON::JSON &data, FLOAT32 sender_lat, FLOAT32 sender_lon)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;
	int type = msg->type();
	if (type != 6 && type != 8 && type != 12 && type != 14 && type != 23)
		return -1;

	Item item;
	item.type = type;
	item.mmsi = msg->mmsi();
	std::string name;
	int linkage = 0;
	KeyKind best = BK_SKIP;
	// a rectangle's corners, for the areas that come as two points (type 23)
	FLOAT32 ne_lat = LAT_UNDEFINED, ne_lon = LON_UNDEFINED, sw_lat = LAT_UNDEFINED, sw_lon = LON_UNDEFINED;

	{
		JSON::Writer w(item.json, 512);
		w.beginObject().kv("mmsi", item.mmsi);
		for (const auto &p : data.getMembers())
		{
			const JSON::Value &val = p.Get();
			int key = p.Key();
			float scale;
			KeyKind k = classify(key, scale);
			if (k == BK_SKIP)
				continue;
			if (key == AIS::KEY_TEXT && val.isString())
			{
				const std::string &t = val.getString();
				if (t.empty() || t == "ONWAON" || t == "ONWAOFF")
					continue;
			}
			switch (key)
			{
			case AIS::KEY_DAC: item.dac = val.getInt(); break;
			case AIS::KEY_FID: item.fi = val.getInt(); break;
			case AIS::KEY_MESSAGE_ID: item.sub = val.getInt(); break;
			case AIS::KEY_LAT: item.lat = val.getFloat(); break;
			case AIS::KEY_LON: item.lon = val.getFloat(); break;
			case AIS::KEY_DEST_MMSI: item.anchor = (uint32_t)val.getInt(); break;
			case AIS::KEY_LINKAGE_ID: linkage = (int)val.getInt(); break;
			case AIS::KEY_AREA_NOTICE_LAT: item.lat = val.getFloat(); break;
			case AIS::KEY_AREA_NOTICE_LON: item.lon = val.getFloat(); break;
			case AIS::KEY_NE_LAT: ne_lat = val.getFloat(); break;
			case AIS::KEY_NE_LON: ne_lon = val.getFloat(); break;
			case AIS::KEY_SW_LAT: sw_lat = val.getFloat(); break;
			case AIS::KEY_SW_LON: sw_lon = val.getFloat(); break;
			case AIS::KEY_STATION_ID:
				if (!val.isString())
					continue;
			case AIS::KEY_LOCK_ID:
				name = val.getString();
				item.label = name;
				break;
			case AIS::KEY_TEXT: case AIS::KEY_VESSEL_NAME: case AIS::KEY_BERTH_NAME:
			case AIS::KEY_STATION_NAME: case AIS::KEY_NAME: case AIS::KEY_AREA_NOTICE_NAME:
				name = val.getString();
				break;
			case AIS::KEY_AREA_SHAPES:
				if (val.isString())
					item.shapes = val.getString();
				break;
			case AIS::KEY_AREA_NOTICE_TYPE:
				item.atype = val.getInt();
				break;
			}
			best = MAX(best, k);

			const AIS::KeyStr &jkey = AIS::KeyMap[key][JSON_DICT_FULL];
			if (val.isString())
				w.kv(jkey, val.getString());
			else if (val.isBool())
				w.kv(jkey, val.getBool());
			else if (val.isInt() && scale == 1)
				w.kv(jkey, (long long)val.getInt());
			else if (val.isFloat() || val.isInt())
				w.kv(jkey, (double)(val.getFloat() * scale));
		}
		w.endObject();
	}
	if (best < BK_TEXT)
		return -1;

	item.kind = (Item::Kind)(best - BK_TEXT);
	// safety-related messages (12, 14) are their own kind
	if (type == 12 || type == 14)
		item.kind = Item::SAFETY;
	item.hash = hashText(name);
	if (item.kind == Item::AREA)
		item.sub = linkage;
	// two corners become the rectangle record an area notice carries; the item stands at the centre
	if (item.kind == Item::AREA && item.shapes.empty() && isValidCoord(ne_lat, ne_lon) && isValidCoord(sw_lat, sw_lon) && ne_lat > sw_lat && ne_lon > sw_lon)
	{
		item.lat = (ne_lat + sw_lat) / 2;
		item.lon = (ne_lon + sw_lon) / 2;
		double east = (ne_lon - sw_lon) * 111320.0 * std::cos(sw_lat * 3.14159265358979323846 / 180.0);
		double north = (ne_lat - sw_lat) * 111320.0;
		char rect[96];
		snprintf(rect, sizeof(rect), "r,%.5f,%.5f,%.0f,%.0f,0", sw_lon, sw_lat, east, north);
		item.shapes = rect;
	}
	if (!isValidCoord(item.lat, item.lon))
		item.lat = item.lon = LAT_UNDEFINED;
	else if (name.empty())
		item.hash = hashPosition(item.hash, item.lat, item.lon);
	// the transmitter is part of the identity for what is about the transmitter, and for
	// anything with neither a name nor a place; a placed sensor reading merges across towers
	if (aboutSender(item.kind) || (name.empty() && !isValidCoord(item.lat, item.lon)))
		item.sender = item.mmsi;

	item.key = (item.hash ^ ((uint64_t)item.kind << 48) ^ ((uint64_t)(item.dac & 0x3FF) << 38) ^ ((uint64_t)(item.fi & 0x3F) << 32) ^ ((uint64_t)(uint16_t)item.sub << 24) ^ item.sender ^ ((uint64_t)item.anchor << 8)) & 0x1FFFFFFFFFFFFFULL;
	if (!item.key)
		item.key = 1;

	if (items.capacity() == 0)
		return -1;

	{
		size_t a = item.label.find_first_not_of(' '), z = item.label.find_last_not_of(' ');
		item.label = a == std::string::npos ? "" : item.label.substr(a, std::min<size_t>(z - a + 1, 8));
	}

	std::time_t now = msg->getRxTimeUnix();
	int h = items.find(item.key);
	if (h != items.NIL)
	{
		Item &b = items[h];
		b.json.swap(item.json);
		b.label.swap(item.label);
		b.shapes.swap(item.shapes);
		b.atype = item.atype;
		b.mmsi = item.mmsi;
		b.lat = item.lat;
		b.lon = item.lon;
		b.count++;
		b.last = now;
		b.seq = ++next_seq;
		items.touch(h);
	}
	else
	{
		h = items.create(item.key);
		// the recycled slot may still be chained somewhere
		unlinkFromMarker(h);
		unlinkFromOwner(h);
		unlinkFromSent(h);
		Item &b = items[h];
		b = item;
		b.count = 1;
		b.first = b.last = now;
		b.seq = ++next_seq;
	}

	// a located item stands on its own marker unless it sits on its sender;
	// everything else belongs to a ship (addressee or sender) and has no marker.
	// Whatever it is, the transmitter keeps it on its sent chain.
	Item &b = items[h];
	if (b.sent_by != b.mmsi)
	{
		unlinkFromSent(h);
		linkToSent(h, b.mmsi);
	}
	bool located = isValidCoord(b.lat, b.lon);
	bool onSender = located && isValidCoord(sender_lat, sender_lon) &&
					std::hypot(b.lat - sender_lat, b.lon - sender_lon) <= SNAP_DEG;
	uint64_t want = (located && !onSender) ? markerKey(b) : 0;
	if (b.marker >= 0 && (!want || markers.key(b.marker) != want))
		unlinkFromMarker(h);
	if (want && b.marker < 0)
		linkToMarker(h, want);
	return h;
}

inline void BinaryStore::writeJSON(JSON::Writer &w, std::time_t now, std::time_t since, uint64_t marker_key, uint32_t owner_mmsi) const
{
	w.beginObject().kv("time", now).kv("timeout", ttl).key("messages").beginArray();

	auto row = [&](const Item &b) {
		w.beginObject().kv("key", (long long)b.key).kv("seq", (long long)b.seq).kv("type", b.type).kv("dac", b.dac).kv("fi", b.fi).kv("timestamp", (long long)b.last).kv("first", (long long)b.first).kv("count", b.count).kv("ttl", ttl).kv("sender", b.mmsi);
		if (b.anchor)
			w.kv("anchor", b.anchor);
		w.kv_raw("message", b.json).endObject();
	};

	if (marker_key)
	{
		int m = markers.find(marker_key);
		if (m != markers.NIL)
			for (int p = markers[m].head; p >= 0; p = items[p].next)
				if ((long int)now - (long int)items[p].last <= ttl)
					row(items[p]);
	}
	else if (owner_mmsi)
	{
		// about the ship and sent by it, an item on both chains once; newest first,
		// capped so a station addressing hundreds of vessels stays a small answer
		int c = owners.find(owner_mmsi);
		if (c != owners.NIL)
		{
			std::vector<int> rows;
			for (int p = owners[c].head; p >= 0; p = items[p].onext)
				if ((long int)now - (long int)items[p].last <= ttl)
					rows.push_back(p);
			for (int p = owners[c].sent; p >= 0; p = items[p].snext)
				if (items[p].owner != owner_mmsi && (long int)now - (long int)items[p].last <= ttl)
					rows.push_back(p);
			std::sort(rows.begin(), rows.end(), [&](int a, int b) { return items[a].last > items[b].last; });
			if (rows.size() > (size_t)MAX_PER_SHIP)
				rows.resize(MAX_PER_SHIP);
			for (int p : rows)
				row(items[p]);
		}
	}
	else
		items.forEach([&](int h) {
			const Item &b = items[h];
			if ((long int)now - (long int)b.last > ttl || (since > 0 && b.last < since))
				return false;
			if (b.marker < 0)
				row(b);
			return true;
		});
	w.endArray().endObject();
}

// Re-aggregates every marker from its live members: expired members are
// dropped, an emptied marker is removed with a tombstone, a changed one
// takes the next sequence number and moves to the front of the walk.
inline void BinaryStore::refresh(std::time_t now)
{
	std::vector<int> changed;
	std::vector<uint64_t> dead;

	markers.forEach([&](int mh) {
		Marker &m = markers[mh];
		uint32_t count = 0;
		int newest = -1;
		time_t first = 0;
		std::vector<std::pair<uint32_t, uint32_t>> senders;

		int p = m.head;
		while (p >= 0)
		{
			int n = items[p].next;
			const Item &it = items[p];
			if ((long int)now - (long int)it.last > ttl)
			{
				unlinkFromMarker(p);
				p = n;
				continue;
			}
			count += it.count;
			if (newest < 0 || it.last > items[newest].last)
				newest = p;
			if (!first || it.first < first)
				first = it.first;
			bool seen = false;
			for (auto &sc : senders)
				if (sc.first == it.mmsi)
				{
					sc.second += it.count;
					seen = true;
				}
			if (!seen)
				senders.push_back(std::make_pair(it.mmsi, it.count));
			p = n;
		}
		if (count == 0)
		{
			dead.push_back(m.key);
			return true;
		}
		const Item &nw = items[newest];
		bool moved = m.lat != nw.lat || m.lon != nw.lon;
		bool updated = m.count != count || m.last != nw.last || m.label != nw.label || m.senders != senders || m.shapes != nw.shapes;
		if (m.fresh || moved || updated)
		{
			m.flags = m.fresh ? Marker::NEW : (moved ? Marker::MOVED : Marker::UPDATED);
			m.fresh = false;
			m.kind = nw.kind;
			m.lat = nw.lat;
			m.lon = nw.lon;
			m.label = nw.label;
			m.shapes = nw.shapes;
			m.atype = nw.atype;
			m.count = count;
			m.first = first;
			m.last = nw.last;
			m.senders.swap(senders);
			m.seq = ++next_seq;
			changed.push_back(mh);
		}
		return true;
	});
	for (uint64_t k : dead)
	{
		tombstone(k, ++next_seq);
		markers.remove(k);
	}
	// touched last = newest in the walk, so ascending sequence order
	for (int mh : changed)
		markers.touch(mh);

	// ship chains shed their expired items too, or a busy station's chain grows until its slots recycle
	auto expired = [&](int p) { return (long int)now - (long int)items[p].last > ttl; };
	std::vector<uint32_t> stale;
	owners.forEach([&](int c) {
		bool any = false;
		for (int p = owners[c].head; p >= 0 && !any; p = items[p].onext)
			any = expired(p);
		for (int p = owners[c].sent; p >= 0 && !any; p = items[p].snext)
			any = expired(p);
		if (any)
			stale.push_back(owners.key(c));
		return true;
	});
	// outside the walk: unlinking the last item removes the record itself
	for (uint32_t m : stale)
	{
		int c = owners.find(m);
		if (c == owners.NIL)
			continue;
		for (int p = owners[c].head; p >= 0;)
		{
			int n = items[p].onext;
			if (expired(p))
				unlinkFromOwner(p);
			p = n;
		}
		c = owners.find(m);
		if (c == owners.NIL)
			continue;
		for (int p = owners[c].sent; p >= 0;)
		{
			int n = items[p].snext;
			if (expired(p))
				unlinkFromSent(p);
			p = n;
		}
	}
}

inline void BinaryStore::writeMarkerRows(JSON::Writer &w, uint64_t since) const
{
	char id[24];
	markers.forEach([&](int mh) {
		const Marker &m = markers[mh];
		if (m.seq <= since)
			return false;
		snprintf(id, sizeof(id), "%llx", (unsigned long long)m.key);
		w.beginObject().kv("id", id).kv("kind", (int)m.kind).kv("flags", (int)m.flags).kv("seq", (long long)m.seq)
			.kv("lat", m.lat).kv("lon", m.lon).kv("label", m.label).kv("count", m.count)
			.kv("t", (long long)m.last).kv("first", (long long)m.first).kv("ttl", ttl);
		if (!m.shapes.empty())
			w.kv("shapes", m.shapes).kv("atype", m.atype);
		w.key("senders").beginArray();
		for (const auto &sc : m.senders)
			w.beginArray().val(sc.first).val(sc.second).endArray();
		w.endArray().endObject();
		return true;
	});
}

inline void BinaryStore::writeRemoved(JSON::Writer &w, uint64_t since) const
{
	char id[24];
	for (const auto &ts : tombstones)
		if (ts.second > since)
		{
			snprintf(id, sizeof(id), "%llx", (unsigned long long)ts.first);
			w.val(id);
		}
}
