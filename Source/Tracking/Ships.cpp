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

	path_ptr = -1;

	mmsi = count = msg_type = shiptype = group_mask = 0;
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
	last_group = GROUP_OUT_UNDEFINED;

	msg.clear();
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

bool Ship::getKML(std::string &kmlString) const
{
	if (lat == LAT_UNDEFINED || lon == LON_UNDEFINED || (lat == 0 && lon == 0))
		return false;

	std::string shipNameStr(shipname);

	const std::string name = !shipNameStr.empty() ? shipNameStr : std::to_string(mmsi);
	const std::string styleId = "style" + std::to_string(mmsi);
	const std::string coordinates = std::to_string(lon) + "," + std::to_string(lat) + ",0";

	kmlString += "<Style id=\"" + styleId + "\"><IconStyle><scale>1</scale><heading>" +
				 std::to_string(cog) + "</heading><Icon><href>/icons.png</href>" +
				 getSprite(this) + "</Icon></IconStyle></Style><Placemark><name>" +
				 name + "</name><description>Description of your placemark</description><styleUrl>#" +
				 styleId + "</styleUrl><Point><coordinates>" +
				 coordinates + "</coordinates></Point></Placemark>";
	return true;
}

bool Ship::getGeoJSON(JSON::Writer &w) const
{
	w.beginObject().kv("type", "Feature").key("properties").beginObject()
		.kv("mmsi", mmsi).kv("distance", distance).kv("bearing", angle)
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
	if (msg_type & ATON_MASK || (mmsi >= 990000000 && mmsi <= 999999999))
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
	if (msg_type & BASESTATION_MASK || (mmsi < 9000000))
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
		&& W(last_group)
		&& W(incoming.next) && W(incoming.prev) && W(path_ptr));
}

bool Ship::Load(std::ifstream &file)
{
	int magic = 0, version = 0;

	if (!R(magic) || !R(version) || magic != _SHIP_MAGIC || version != _SHIP_VERSION)
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
		&& R(last_group)
		&& R(incoming.next) && R(incoming.prev) && R(path_ptr));

	// msg vector is not persisted
	msg.clear();
	return ok;
}

#undef W
#undef R
