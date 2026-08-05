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

#include <vector>
#include <fstream>

#include "AIS-catcher.h"
#include "Ships.h"
#include "Writer.h"

void Ship::reset()
{
	mmsi = count = msg_type = shiptype = group_mask = 0;
	type_ttl = 0;
	flags.reset();

	heading = HEADING_UNDEFINED;
	status = STATUS_UNDEFINED;
	to_port = to_bow = to_starboard = to_stern = DIMENSION_UNDEFINED;
	IMO = IMO_UNDEFINED;
	angle = ANGLE_UNDEFINED;
	month = ETA_MONTH_UNDEFINED;
	day = ETA_DAY_UNDEFINED;
	hour = ETA_HOUR_UNDEFINED;
	minute = ETA_MINUTE_UNDEFINED;
	lat = LAT_UNDEFINED;
	lon = LON_UNDEFINED;
	ppm = PPM_UNDEFINED;
	level = LEVEL_UNDEFINED;
	altitude = ALT_UNDEFINED;
	received_stations = RECEIVED_STATIONS_UNDEFINED;

	distance = DISTANCE_UNDEFINED;
	angle = ANGLE_UNDEFINED;
	draught = DRAUGHT_UNDEFINED;
	speed = SPEED_UNDEFINED;

	cog = COG_UNDEFINED;
	last_signal = last_direct_signal = last_static_signal = {};
	shipclass = CLASS_UNKNOWN;
	mmsi_type = MMSI_OTHER;

	memset(shipname, 0, sizeof(shipname));
	memset(destination, 0, sizeof(destination));
	memset(callsign, 0, sizeof(callsign));
	memset(country_code, 0, sizeof(country_code));
	memset(vin, 0, sizeof(vin));
	memset(vendorid, 0, sizeof(vendorid));
	unit_model = unit_serial = -1;
	last_group = GROUP_OUT_UNDEFINED;

	msg.clear();
}

// Which fields each message type can refresh, indexed by type. A missing field
// here expires early, a spurious one never expires.
static const uint32_t TYPE_FIELDS[MAX_MSG_TYPE + 1] = {
	0,																			// 0  does not exist
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_HEADING | F_STATUS | F_MANEUVER | F_RECV_STATIONS, // 1  position report
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_HEADING | F_STATUS | F_MANEUVER | F_RECV_STATIONS, // 2  position report
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_HEADING | F_STATUS | F_MANEUVER | F_RECV_STATIONS, // 3  position report
	F_SIGNAL | F_LATLON | F_RECV_STATIONS,										// 4  base station report
	F_SIGNAL | F_VOYAGE | F_STATIC,												// 5  static and voyage
	F_SIGNAL | F_LATLON | F_OFF_POSITION | F_STATIC | F_VOYAGE,					// 6  addressed binary, buoy/AtoN monitor payloads
	F_SIGNAL,																	// 7  binary acknowledge
	F_SIGNAL | F_STATIC | F_VOYAGE | F_OFF_POSITION,							// 8  broadcast binary, inland static and AtoN monitor payloads
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_ALTITUDE,								// 9  SAR aircraft
	F_SIGNAL,																	// 10 UTC inquiry
	F_SIGNAL | F_LATLON | F_RECV_STATIONS,										// 11 UTC response
	F_SIGNAL,																	// 12 addressed safety
	F_SIGNAL,																	// 13 safety acknowledge
	F_SIGNAL,																	// 14 broadcast safety
	F_SIGNAL,																	// 15 interrogation
	F_SIGNAL,																	// 16 assignment
	F_SIGNAL,																	// 17 DGNSS broadcast
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_HEADING | F_COMM_CAP,					// 18 class B position
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_HEADING | F_STATIC,					// 19 class B extended
	F_SIGNAL,																	// 20 link management
	F_SIGNAL | F_LATLON | F_OFF_POSITION | F_STATIC,							// 21 aid to navigation
	F_SIGNAL,																	// 22 channel management
	F_SIGNAL,																	// 23 group assignment
	F_SIGNAL | F_STATIC,														// 24 class B static
	F_SIGNAL | F_LATLON | F_OFF_POSITION | F_STATIC | F_VOYAGE,					// 25 single slot binary, same payloads as 6/8
	F_SIGNAL | F_LATLON | F_OFF_POSITION | F_STATIC | F_VOYAGE | F_RECV_STATIONS, // 26 multiple slot binary, same payloads as 6/8
	F_SIGNAL | F_LATLON | F_SPEED_COG | F_STATUS,								// 27 long range
	F_SIGNAL | F_LATLON | F_STATIC,												// 28 AtoN report
};

void Ship::decayAndExpire()
{
	if (~type_ttl & msg_type)
	{
		uint32_t supported = 0;
		for (int t = 1; t <= MAX_MSG_TYPE; t++)
			if (type_ttl & (1 << t))
				supported |= TYPE_FIELDS[t];

		clearFields(~supported);

		msg_type = type_ttl;
		setType();
	}

	type_ttl = 0;
}

void Ship::clearFields(uint32_t doomed)
{
	if (doomed & F_LATLON)
	{
		lat = LAT_UNDEFINED;
		lon = LON_UNDEFINED;
		distance = DISTANCE_UNDEFINED;
		angle = ANGLE_UNDEFINED;
		setApproximate(0);
		setValidated(0);
		setRAIM(0);
		setAssigned(0);
	}
	if (doomed & F_SPEED_COG)
	{
		speed = SPEED_UNDEFINED;
		cog = COG_UNDEFINED;
	}
	if (doomed & F_HEADING)
		heading = HEADING_UNDEFINED;
	if (doomed & F_STATUS)
		status = STATUS_UNDEFINED;
	if (doomed & F_MANEUVER)
		setManeuver(0);
	if (doomed & F_ALTITUDE)
		altitude = ALT_UNDEFINED;
	if (doomed & F_RECV_STATIONS)
		received_stations = RECEIVED_STATIONS_UNDEFINED;
	if (doomed & F_OFF_POSITION)
		setOffPosition(0);
	if (doomed & F_VOYAGE)
	{
		memset(destination, 0, sizeof(destination));
		month = ETA_MONTH_UNDEFINED;
		day = ETA_DAY_UNDEFINED;
		hour = ETA_HOUR_UNDEFINED;
		minute = ETA_MINUTE_UNDEFINED;
		draught = DRAUGHT_UNDEFINED;
	}
	if (doomed & F_STATIC)
	{
		memset(shipname, 0, sizeof(shipname));
		memset(callsign, 0, sizeof(callsign));
		memset(vendorid, 0, sizeof(vendorid));
		memset(vin, 0, sizeof(vin));
		unit_model = unit_serial = -1;
		IMO = IMO_UNDEFINED;
		shiptype = 0;
		to_port = to_bow = to_starboard = to_stern = DIMENSION_UNDEFINED;
		setDTE(0);
		setVirtualAid(0);
	}
	if (doomed & F_COMM_CAP)
	{
		setCSUnit(0);
		setDisplay(0);
		setDSC(0);
		setBand(0);
		setMsg22(0);
	}
	if (doomed & F_SIGNAL)
	{
		ppm = PPM_UNDEFINED;
		level = LEVEL_UNDEFINED;
		setRepeat(0);
		memset(country_code, 0, sizeof(country_code));
		clearOpChannels();
		msg.clear();
	}
}

std::string getSprite(const Ship *ship)
{
	std::string shipofs = (ship->speed != SPEED_UNDEFINED && ship->speed > 0.5) ? "<y>88</y><w>20</w><h>20</h>" : "<y>68</y><w>20</w><h>20</h>";
	std::string stationofs = "<y>50</y><w>20</w><h>20</h>";

	switch (ship->shipclass)
	{
	case CLASS_OTHER:
		return "<x>120</x>" + shipofs;
	case CLASS_UNKNOWN:
		return "<x>120</x>" + shipofs;
	case CLASS_CARGO:
		return "<x>0</x>" + shipofs;
	case CLASS_TANKER:
		return "<x>80</x>" + shipofs;
	case CLASS_PASSENGER:
		return "<x>40</x>" + shipofs;
	case CLASS_HIGHSPEED:
		return "<x>100</x>" + shipofs;
	case CLASS_SPECIAL:
		return "<x>60</x>" + shipofs;
	case CLASS_FISHING:
		return "<x>140</x>" + shipofs;
	case CLASS_B:
		return "<x>140</x>" + shipofs;
	case CLASS_ATON:
		return "<x>0</x>" + stationofs;
	case CLASS_STATION:
		return "<x>20</x>" + stationofs;
	case CLASS_SARTEPIRB:
		return "<x>40</x>" + stationofs;
	case CLASS_HELICOPTER:
		return "<w>25</w><h>25</h>";
	case CLASS_PLANE:
		return "<y>25</y><w>25</w><h>25</h>";
	}
	return "";
}

static void appendXMLEscaped(std::string &out, const std::string &s)
{
	for (char c : s)
		switch (c)
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		default: out += c;
		}
}

bool Ship::getKML(std::string &kmlString) const
{
	if (!isValidCoord(lat, lon))
		return false;

	std::string shipNameStr(shipname);

	const std::string name = !shipNameStr.empty() ? shipNameStr : std::to_string(mmsi);
	const std::string styleId = "style" + std::to_string(mmsi);
	const std::string coordinates = std::to_string(lon) + "," + std::to_string(lat) + ",0";

	kmlString += "<Style id=\"" + styleId + "\"><IconStyle><scale>1</scale><heading>" +
				 std::to_string(cog) + "</heading><Icon><href>/icons.png</href>" +
				 getSprite(this) + "</Icon></IconStyle></Style><Placemark><name>";
	appendXMLEscaped(kmlString, name);
	kmlString += "</name><styleUrl>#" +
				 styleId + "</styleUrl><Point><coordinates>" +
				 coordinates + "</coordinates></Point></Placemark>";
	return true;
}

bool Ship::getGeoJSON(JSON::Writer &w, bool station_known) const
{
	w.beginObject().kv("type", "Feature").key("properties").beginObject()
		.kv("mmsi", mmsi);
	if (station_known)
		w.kv("distance", distance).kv("bearing", angle);
	else
		w.kv_null("distance").kv_null("bearing");
	w
		.kv_unless("level", level, LEVEL_UNDEFINED).kv("count", count)
		.kv_unless("ppm", ppm, PPM_UNDEFINED).kv("group_mask", group_mask)
		.kv("approx", (bool)getApproximate())
		.kv_unless("heading", heading, HEADING_UNDEFINED).kv_unless("cog", cog, COG_UNDEFINED).kv_unless("speed", speed, SPEED_UNDEFINED)
		.kv_unless("to_bow", to_bow, DIMENSION_UNDEFINED).kv_unless("to_stern", to_stern, DIMENSION_UNDEFINED)
		.kv_unless("to_starboard", to_starboard, DIMENSION_UNDEFINED).kv_unless("to_port", to_port, DIMENSION_UNDEFINED)
		.kv("shiptype", shiptype).kv("mmsi_type", mmsi_type).kv("shipclass", shipclass)
		.kv("validated", getValidated()).kv("msg_type", msg_type).kv("channels", getChannels())
		.kv("country", country_code).kv("status", status).kv("draught", draught)
		.kv_unless("eta_month", (int)month, ETA_MONTH_UNDEFINED).kv_unless("eta_day", (int)day, ETA_DAY_UNDEFINED)
		.kv_unless("eta_hour", (int)hour, ETA_HOUR_UNDEFINED).kv_unless("eta_minute", (int)minute, ETA_MINUTE_UNDEFINED)
		.kv_unless("imo", IMO, IMO_UNDEFINED).kv("callsign", callsign);

	if (getVirtualAid())
		w.kv("shipname", shipname, " [V]");
	else
		w.kv("shipname", shipname);

	w.kv("destination", destination).kv("last_signal", last_signal).endObject()
		.key("geometry").beginObject().kv("type", "Point")
			.key("coordinates").beginArray().val(lon).val(lat).endArray()
			.endObject()
		.endObject();

	return true;
}

int Ship::getMMSItype()
{
	if ((mmsi > 111000000 && mmsi < 111999999) || (mmsi > 11100000 && mmsi < 11199999))
	{
		return MMSI_SAR;
	}
	if (mmsi >= 970000000 && mmsi <= 980000000)
	{
		return MMSI_SARTEPIRB;
	}
	// the MMSI number outranks the message types
	if (mmsi >= 990000000 && mmsi <= 999999999)
	{
		return MMSI_ATON;
	}
	if (mmsi < 9000000)
	{
		return MMSI_BASESTATION;
	}
	if (msg_type & ATON_MASK)
	{
		return MMSI_ATON;
	}
	if (msg_type & CLASS_A_MASK)
	{
		return MMSI_CLASS_A;
	}
	if (msg_type & CLASS_B_MASK)
	{
		return MMSI_CLASS_B;
	}
	if (msg_type & BASESTATION_MASK)
	{
		return MMSI_BASESTATION;
	}
	if (msg_type & SAR_MASK)
	{
		return MMSI_SAR;
	}
	if (msg_type & CLASS_A_STATIC_MASK)
	{
		return MMSI_CLASS_A;
	}
	if (msg_type & CLASS_B_STATIC_MASK)
	{
		return MMSI_CLASS_B;
	}
	return MMSI_OTHER;
}

int Ship::getShipTypeClassEri()
{
	switch (shiptype)
	{
	// Cargo cases
	case 8030:
	case 8010:
	case 8070:
	case 8210:
	case 8220:
	case 8230:
	case 8240:
	case 8250:
	case 8260:
	case 8270:
	case 8280:
	case 8290:
	case 8310:
	case 8320:
	case 8330:
	case 8340:
	case 8350:
	case 8360:
	case 8370:
	case 8380:
	case 8390:
	case 8130:
	case 8140:
	case 8150:
	case 8170:
	case 8410:
	case 1500:
	case 1510:
	case 1520:
		return CLASS_CARGO;
	// Tanker cases
	case 8020:
	case 8021:
	case 8022:
	case 8023:
	case 8040:
	case 8060:
	case 8160:
	case 8161:
	case 8162:
	case 8163:
	case 8180:
	case 8490:
	case 8500:
	case 1530:
	case 1540:
		return CLASS_TANKER;
	// Special cases
	case 8050:
	case 8080:
	case 8090:
	case 8100:
	case 8110:
	case 8120:
	case 8400:
	case 8420:
	case 8430:
	case 8450:
	case 8451:
	case 8452:
	case 8453:
	case 8454:
	case 8460:
	case 8470:
	case 8510:
		return CLASS_SPECIAL;
	// Passenger cases
	case 8440:
	case 8441:
	case 8442:
	case 8443:
	case 8444:
	case 8445:
	case 8446:
	case 8447:
	case 8448:
		return CLASS_PASSENGER;
	// Other cases
	case 8480:
		return CLASS_FISHING;
	case 1850:
		return CLASS_B;
	case 1900:
	case 1910:
	case 1920:
		return CLASS_HIGHSPEED;
	default:
		return CLASS_OTHER;
	}
}

int Ship::getShipTypeClass()
{
	int c = CLASS_UNKNOWN;

	switch (mmsi_type)
	{
	case MMSI_CLASS_A:
	case MMSI_CLASS_B:
		c = (mmsi_type == MMSI_CLASS_B) ? CLASS_B : CLASS_UNKNOWN;

		// Overwrite default if there's a better mapping
		if (shiptype >= 80 && shiptype < 90)
			c = CLASS_TANKER;
		else if (shiptype >= 70 && shiptype < 80)
			c = CLASS_CARGO;
		else if (shiptype >= 60 && shiptype < 70)
			c = CLASS_PASSENGER;
		else if (shiptype >= 40 && shiptype < 50)
			c = CLASS_HIGHSPEED;
		else if (shiptype >= 50 && shiptype < 60)
			c = CLASS_SPECIAL;
		else if (shiptype == 30)
			c = CLASS_FISHING;
		else if ((shiptype >= 1500 && shiptype <= 1920) || (shiptype >= 8000 && shiptype <= 8510))
		{
			c = getShipTypeClassEri();
		}
		break;
	case MMSI_BASESTATION:
		c = CLASS_STATION;
		break;
	case MMSI_SAR:
		c = CLASS_HELICOPTER;
		if ((mmsi > 111000000 && mmsi < 111999999 && (mmsi / 100) % 10 == 1) || (mmsi > 11100000 && mmsi < 11199999 && (mmsi / 10) % 10 == 1))
			c = CLASS_PLANE;
		break;
	case MMSI_SARTEPIRB:
		c = CLASS_SARTEPIRB;
		break;
	case MMSI_ATON:
		c = CLASS_ATON;
		break;
	default:
		break;
	}

	return c;
}

void Ship::setType()
{
	mmsi_type = getMMSItype();
	shipclass = getShipTypeClass();
}

#define W(x) file.write((const char *)&(x), sizeof(x))
#define R(x) file.read((char *)&(x), sizeof(x))

bool Ship::Save(std::ofstream &file) const
{
	int magic = _SHIP_MAGIC;
	int version = _SHIP_VERSION;

	return (bool)(W(magic) && W(version)
		&& W(mmsi) && W(count) && W(msg_type) && W(shiptype) && W(group_mask) && W(flags)
		&& W(heading) && W(status)
		&& W(to_port) && W(to_bow) && W(to_starboard) && W(to_stern)
		&& W(IMO) && W(angle)
		&& W(month) && W(day) && W(hour) && W(minute)
		&& W(lat) && W(lon) && W(ppm) && W(level) && W(altitude) && W(received_stations)
		&& W(distance) && W(draught) && W(speed) && W(cog)
		&& W(last_signal) && W(last_direct_signal)
		&& W(shipclass) && W(mmsi_type)
		&& W(shipname) && W(destination) && W(callsign) && W(country_code) && W(vin)
		&& W(vendorid) && W(unit_model) && W(unit_serial)
		&& W(last_group));
}

bool Ship::Load(std::ifstream &file)
{
	int magic = 0, version = 0;

	if (!R(magic) || !R(version) || magic != _SHIP_MAGIC)
		return false;

	if (version != _SHIP_VERSION && version != _SHIP_VERSION_LINKED)
		return false;

	bool ok = (bool)(R(mmsi) && R(count) && R(msg_type) && R(shiptype) && R(group_mask) && R(flags)
		&& R(heading) && R(status)
		&& R(to_port) && R(to_bow) && R(to_starboard) && R(to_stern)
		&& R(IMO) && R(angle)
		&& R(month) && R(day) && R(hour) && R(minute)
		&& R(lat) && R(lon) && R(ppm) && R(level) && R(altitude) && R(received_stations)
		&& R(distance) && R(draught) && R(speed) && R(cog)
		&& R(last_signal) && R(last_direct_signal)
		&& R(shipclass) && R(mmsi_type)
		&& R(shipname) && R(destination) && R(callsign) && R(country_code) && R(vin)
		&& R(vendorid) && R(unit_model) && R(unit_serial)
		&& R(last_group));

	if (ok && version == _SHIP_VERSION_LINKED)
	{
		int discard[3];
		ok = (bool)R(discard);
	}

	// msg vector is not persisted
	msg.clear();
	return ok;
}

#undef W
#undef R

void Ship::writeCompactDynamic(JSON::Writer &w) const
{
	w.beginArray().val(mmsi);
	if (isValidCoord(lat, lon))
	{
		w.val(lat).val(lon);
		if (distance != DISTANCE_UNDEFINED && angle != ANGLE_UNDEFINED)
			w.val(distance).val(angle);
		else
			w.val_null().val_null();
	}
	else
		w.val_null().val_null().val_null().val_null();

	w.val_unless(heading, HEADING_UNDEFINED)
		.val_unless(cog, COG_UNDEFINED)
		.val_unless(speed, SPEED_UNDEFINED)
		.val(status)
		.val_unless(level, LEVEL_UNDEFINED)
		.val_unless(ppm, PPM_UNDEFINED)
		.val(count)
		.val(msg_type)
		.val(last_signal)
		.val(last_group)
		.val(group_mask)
		.val((unsigned long long)flags.getPackedValue())
		.val_unless(altitude, ALT_UNDEFINED)
		.val_unless(received_stations, RECEIVED_STATIONS_UNDEFINED)
		.val(mmsi_type)
		.val(shipclass)
		.val(country_code)
		.endArray();
}

void Ship::writeCompactStatic(JSON::Writer &w) const
{
	w.beginArray().val(mmsi);

	if (getVirtualAid())
		w.val(shipname, " [V]");
	else
		w.val(shipname);

	w.val(callsign)
		.val(destination)
		.val(shiptype)
		.val_unless(IMO, IMO_UNDEFINED)
		.val_unless(to_bow, DIMENSION_UNDEFINED)
		.val_unless(to_stern, DIMENSION_UNDEFINED)
		.val_unless(to_port, DIMENSION_UNDEFINED)
		.val_unless(to_starboard, DIMENSION_UNDEFINED)
		.val_unless(draught, DRAUGHT_UNDEFINED)
		.val_unless((int)month, ETA_MONTH_UNDEFINED)
		.val_unless((int)day, ETA_DAY_UNDEFINED)
		.val_unless((int)hour, ETA_HOUR_UNDEFINED)
		.val_unless((int)minute, ETA_MINUTE_UNDEFINED)
		.val(vin)
		.val(vendorid)
		.val_unless(unit_model, -1)
		.val_unless(unit_serial, -1)
		.endArray();
}

void Ship::getJSON(JSON::Writer &w, long int delta_time, bool station_known) const
{
	w.beginObject().kv("mmsi", mmsi);

	if (isValidCoord(lat, lon))
	{
		w.kv("lat", lat).kv("lon", lon);
		if (station_known)
			w.kv("distance", distance).kv("bearing", angle);
		else
			w.kv_null("distance").kv_null("bearing");
	}
	else
		w.kv_null("lat").kv_null("lon").kv_null("distance").kv_null("bearing");

	w.kv_unless("level", level, LEVEL_UNDEFINED)
		.kv("count", count)
		.kv_unless("ppm", ppm, PPM_UNDEFINED)
		.kv("group_mask", group_mask)
		.kv("approx", (bool)getApproximate())
		.kv_unless("heading", heading, HEADING_UNDEFINED)
		.kv_unless("cog", cog, COG_UNDEFINED)
		.kv_unless("speed", speed, SPEED_UNDEFINED)
		.kv_unless("to_bow", to_bow, DIMENSION_UNDEFINED)
		.kv_unless("to_stern", to_stern, DIMENSION_UNDEFINED)
		.kv_unless("to_starboard", to_starboard, DIMENSION_UNDEFINED)
		.kv_unless("to_port", to_port, DIMENSION_UNDEFINED)
		.kv("shiptype", shiptype)
		.kv("mmsi_type", mmsi_type)
		.kv("shipclass", shipclass)
		.kv("validated", getValidated())
		.kv("msg_type", msg_type)
		.kv("channels", getChannels())
		.kv("country", country_code)
		.kv("status", status)
		.kv_unless("draught", draught, DRAUGHT_UNDEFINED)
		.kv_unless("eta_month", (int)month, ETA_MONTH_UNDEFINED)
		.kv_unless("eta_day", (int)day, ETA_DAY_UNDEFINED)
		.kv_unless("eta_hour", (int)hour, ETA_HOUR_UNDEFINED)
		.kv_unless("eta_minute", (int)minute, ETA_MINUTE_UNDEFINED)
		.kv_unless("imo", IMO, IMO_UNDEFINED)
		.kv("callsign", callsign);

	if (getVirtualAid())
		w.kv("shipname", shipname, " [V]");
	else
		w.kv("shipname", shipname);

	w.kv("destination", destination)
		.kv("eni", vin)
		.kv("vendorid", vendorid)
		.kv_unless("model", unit_model, -1)
		.kv_unless("serial", unit_serial, -1)
		.kv("repeat", getRepeat())
		.kv("last_signal", delta_time)
		.endObject();
}
