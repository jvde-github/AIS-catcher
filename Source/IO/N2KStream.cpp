/*
	Copyright(c) 2024 jvde.github@gmail.com

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

// Work in progress. This code is using the NMEA2000 library to properly manage address selection and heartbeat
// The functionality has only been tested on a virtual socketCAN network using CANboat
// The code is not yet ready for production use

// Sources:
// 	https://github.com/canboat/canboat
// 	https://github.com/ttlappalainen/NMEA2000

#include <cstring>

#ifdef HASNMEA2000
#include "JSONAIS.h"
#include "N2KStream.h"
#include "N2KInterface.h"

namespace IO
{

	void N2KStreamer::Start()
	{
		N2K::N2KInterface.setNetwork(dev);
		N2K::N2KInterface.addOutput();
		N2K::N2KInterface.Start();
	}

	void N2KStreamer::Stop()
	{
		N2K::N2KInterface.Stop();
	}

	// Follows https://github.com/AK-Homberger/N2K-AIS-Gateway implementation with some small tweaks

	void N2KStreamer::sendType123(const AIS::Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED, turn = 0;
		int heading = HEADING_UNDEFINED, status = 0, second = 0, raim = 0, accuracy = 0, maneuver = 0, radio = 0;

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_STATUS:
				status = p.Get().getInt();
				break;
			case AIS::KEY_TURN:
				turn = p.Get().getInt();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_MANEUVER:
				maneuver = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		tN2kMsg N2kMsg;

		N2kMsg.SetPGN(129038);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | (ais.type() & 0x3f));
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte((second & 0x3f) << 2 | (raim & 0x01) << 1 | (accuracy & 0x01));
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		N2kMsg.Add2ByteDouble(turn, 3.125E-05 / PI * 180.0f * 60.0f, -128);
		N2kMsg.AddByte((0xff << 6) | (maneuver & 3) << 4 | (status & 0x0f));
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(0xff);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	// based on CANboat PGN description
	void N2KStreamer::sendType4(const AIS::Message &ais, const JSON::JSON *data)
	{
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		int raim = 0, accuracy = 0, hour = 0, minute = 0, second = 0, year = 0, month = 0, day = 0, epfd = 0, days = 0, radio = 0;

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_MINUTE:
				minute = p.Get().getInt();
				break;
			case AIS::KEY_HOUR:
				hour = p.Get().getInt();
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_YEAR:
				year = p.Get().getInt();
				break;
			case AIS::KEY_MONTH:
				month = p.Get().getInt();
				break;
			case AIS::KEY_DAY:
				day = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			}
		}

		tN2kMsg N2kMsg;

		std::time_t now = std::time(nullptr);
		std::tm *utc_tm = std::gmtime(&now);

		if (utc_tm)
		{
			utc_tm->tm_year = year - 1900;
			utc_tm->tm_mon = month - 1;
			utc_tm->tm_mday = day;
			days = std::mktime(utc_tm) / 86400;
		}

		N2kMsg.SetPGN(129793);
		N2kMsg.Priority = 7;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());

		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);

		N2kMsg.AddByte(raim << 1 | accuracy);
		N2kMsg.Add4ByteUInt((hour * 3600 + minute * 60 + second) * 10000);

		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));

		N2kMsg.Add2ByteUInt(days);
		N2kMsg.AddByte(epfd << 4 | 0xf);
		N2kMsg.AddByte(0xff);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType5(const AIS::Message &ais, const JSON::JSON *data)
	{

		int ais_version = 0, IMO = 0, shiptype = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, month = 0, day = 0, hour = 0;
		int minute = 0, epfd = 0, dte = 0, nDays = 0;
		char callsign[7] = {' '}, shipname[20] = {' '}, destination[20] = {' '};
		float draught = 0;

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_IMO:
				IMO = p.Get().getInt();
				break;
			case AIS::KEY_CALLSIGN:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(callsign, s.c_str(), std::min(sizeof(callsign), s.size()));
			}
			break;
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_MONTH:
				month = p.Get().getInt();
				break;
			case AIS::KEY_DAY:
				day = p.Get().getInt();
				break;
			case AIS::KEY_MINUTE:
				minute = p.Get().getInt();
				break;
			case AIS::KEY_HOUR:
				hour = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_AIS_VERSION:
				ais_version = p.Get().getInt();
				break;
			case AIS::KEY_DRAUGHT:
				draught = p.Get().getFloat();
				break;
			case AIS::KEY_DESTINATION:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(destination, s.c_str(), std::min(sizeof(destination), s.size()));
			}
			break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			}
		}

		tN2kMsg N2kMsg;

		if (month && day)
		{
			std::time_t now = std::time(nullptr);
			std::tm *utc_tm = std::gmtime(&now);

			if (utc_tm)
			{
				utc_tm->tm_year += (utc_tm->tm_mon + 1 < month || (utc_tm->tm_mon + 1 == month && day < utc_tm->tm_mday)) ? 1 : 0;
				utc_tm->tm_mon = month - 1;
				utc_tm->tm_mday = day;
				nDays = std::mktime(utc_tm) / 86400;
			}
		}

		N2kMsg.SetPGN(129794L);
		N2kMsg.Priority = 6;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | (ais.type() & 0x3f));
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteUInt(IMO);
		N2kMsg.AddStr(callsign, 7);
		N2kMsg.AddStr(shipname, 20);
		N2kMsg.AddByte(shiptype);
		N2kMsg.Add2ByteDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteDouble(to_port + to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_bow, 0.1);
		N2kMsg.Add2ByteUInt(nDays);
		N2kMsg.Add4ByteUInt((hour * 3600 + minute * 60) * 10000);
		N2kMsg.Add2ByteDouble(draught, 0.01);
		N2kMsg.AddStr(destination, 20);
		N2kMsg.AddByte((dte & 0x01) << 6 | (epfd & 0x0f) << 2 | (ais_version & 0x03));
		N2kMsg.AddByte(0xe0 | (ais.getChannel() == 'A' ? 0 : 1));
		N2kMsg.AddByte(0xff);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType9(const AIS::Message &ais, const JSON::JSON *data)
	{
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED;
		int alt = 0, raim = 0, accuracy = 0, second = 0, reserved = 0, dte = 0, radio = 0, speed = 1023;

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getInt();
				break;
			case AIS::KEY_ALT:
				alt = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_RESERVED:
				reserved = p.Get().getInt();
				break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			}
		}

		tN2kMsg N2kMsg;

		N2kMsg.SetPGN(129798);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());

		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);

		N2kMsg.AddByte(second << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180 / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.1 * 3600.0f / 1852.0f, 1023);

		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));

		N2kMsg.Add4ByteDouble(alt, 0.01, ALT_UNDEFINED);
		N2kMsg.AddByte(reserved);
		N2kMsg.AddByte(dte | 254);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType14(const AIS::Message &ais, const JSON::JSON *data)
	{

		std::string text;
		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_TEXT:
			{
				text = p.Get().getString();
			}
			break;
			}
		}

		tN2kMsg N2kMsg;

		N2kMsg.SetPGN(129802);
		N2kMsg.Priority = 5;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.AddByte(0xe0 | (ais.getChannel() == 'A' ? 0 : 1));
		N2kMsg.AddStr(text.c_str(), text.size());

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType18(const AIS::Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED;
		int heading = HEADING_UNDEFINED, radio = 0, second = 0, raim = 0, accuracy = 0, CS = 0, display = 0, DSC = 0, band = 0, msg22 = 0, assigned = 0 /*, maneuver = 0*/;

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_CS:
				CS = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DISPLAY:
				display = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DSC:
				DSC = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_BAND:
				band = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_MSG22:
				msg22 = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		tN2kMsg N2kMsg;
		N2kMsg.SetPGN(129039);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(second << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		N2kMsg.AddByte(0xff); // Regional application
		N2kMsg.AddByte(assigned << 7 | msg22 << 6 | band << 5 | DSC << 4 | display << 3 | CS << 2);
		N2kMsg.AddByte(0xfe | (radio & 1)); // msg 18 has 20 radio bits, check whether this is correct indeed
		N2kMsg.AddByte(0xff);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType19(const AIS::Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED;
		int heading = HEADING_UNDEFINED, shiptype = 0, to_bow = 0, to_stern = 0, second = 0, to_port = 0, to_starboard = 0, epfd = 0, accuracy = 0, raim = 0, dte = 0, assigned = 0;
		char shipname[20] = {' '};

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		tN2kMsg N2kMsg;

		N2kMsg.SetPGN(129040);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(second << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(shiptype);
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		N2kMsg.AddByte(epfd << 4);
		N2kMsg.Add2ByteDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteDouble(to_port + to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_bow, 0.1);
		N2kMsg.AddStr(shipname, 20);
		N2kMsg.AddByte(dte | assigned << 1);
		N2kMsg.AddByte(0x00);
		N2kMsg.AddByte(0xff);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType21(const AIS::Message &ais, const JSON::JSON *data)
	{

		int aid_type = 0, accuracy = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, epfd = 0, second = 0;
		int off_position = 0, regional = 0, raim = 0, virtual_aid = 0, assigned = 0;
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED;

		char name[20] = {' '};

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_NAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(name, s.c_str(), std::min(sizeof(name), s.size()));
			}
			break;
			case AIS::KEY_AID_TYPE:
				aid_type = p.Get().getInt();
				break;
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_OFF_POSITION:
				off_position = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_REGIONAL:
				regional = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_VIRTUAL_AID:
				virtual_aid = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			}
		}

		tN2kMsg N2kMsg;

		N2kMsg.SetPGN(129041);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(second << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteUDouble(to_starboard + to_port, 0.1);
		N2kMsg.Add2ByteUDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteUDouble(to_bow, 0.1);
		N2kMsg.AddByte(assigned << 7 | virtual_aid << 6 | off_position << 5 | aid_type);
		N2kMsg.AddByte(epfd << 1);
		N2kMsg.AddByte(regional);
		N2kMsg.AddByte(ais.getChannel() == 'A' ? 1 : 0 | 0xe0);
		N2kMsg.AddVarStr(name);

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::sendType24(const AIS::Message &ais, const JSON::JSON *data)
	{

		int shiptype = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, mothership_mmsi = 0, partno = 0;
		char shipname[20] = {' '}, callsign[7] = {' '}, vendorid[7] = {' '};

		for (const JSON::Property &p : data[0].getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_CALLSIGN:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(callsign, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_VENDORID:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(vendorid, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_PARTNO:
				partno = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_MOTHERSHIP_MMSI:
				mothership_mmsi = p.Get().getInt();
				break;
			}
		}

		tN2kMsg N2kMsg;

		if (partno == 0)
		{
			N2kMsg.SetPGN(129809);
			N2kMsg.Priority = 6;
			N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
			N2kMsg.Add4ByteUInt(ais.mmsi());
			N2kMsg.AddStr(shipname, 20);
		}
		else
		{
			N2kMsg.SetPGN(129810);
			N2kMsg.Priority = 6;
			N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
			N2kMsg.Add4ByteUInt(ais.mmsi());
			N2kMsg.AddByte(shiptype);
			N2kMsg.AddStr(vendorid, 7);
			N2kMsg.AddStr(callsign, 7);
			N2kMsg.Add2ByteUDouble(to_bow + to_stern, 0.1);
			N2kMsg.Add2ByteUDouble(to_port + to_starboard, 0.1);
			N2kMsg.Add2ByteUDouble(to_starboard, 0.1);
			N2kMsg.Add2ByteUDouble(to_bow, 0.1);
			N2kMsg.Add4ByteUInt(mothership_mmsi);
			N2kMsg.AddByte(0xff);
			N2kMsg.AddByte(ais.getChannel() == 'A' ? 1 : 0 | 0xe0);
			N2kMsg.AddByte(0xff); // sequence ID
		}

		N2K::N2KInterface.sendMsg(N2kMsg);
	}

	void N2KStreamer::Receive(const JSON::JSON *data, int ln, TAG &tag)
	{
		AIS::Message &ais = *((AIS::Message *)data[0].binary);

		if (filter.include(ais))
		{

			switch (ais.type())
			{
			case 1:
			case 2:
			case 3:
				sendType123(ais, data);
				break;
			case 4:
			case 11:
				sendType4(ais, data);
				break;
			case 5:
				sendType5(ais, data);
				break;
			case 9:
				sendType9(ais, data);
				break;
			case 14:
				sendType14(ais, data);
				break;
			case 18:
				sendType18(ais, data);
				break;
			case 19:
				sendType19(ais, data);
				break;
			case 21:
				sendType21(ais, data);
				break;
			case 24:
				sendType24(ais, data);
				break;
			default:
				break;
			}
			return;
		}
	}

	Setting &N2KStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "GROUPS_IN")
		{
			StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "DEVICE")
		{
			dev = arg;
		}
		else if (!setOption(option, arg) && !filter.SetOption(option, arg))
		{
			throw std::runtime_error("JSON output - unknown option: " + option);
		}
		return *this;
	}
}

#endif