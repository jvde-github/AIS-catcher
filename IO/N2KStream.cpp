/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
// https://github.com/canboat/canboat
// https://github.com/ttlappalainen/NMEA2000
// https://github.com/AK-Homberger/NMEA2000-AIS-Gateway

#include <cstring>

#ifdef HASNMEA2000
#
char socketName[50];
#define SOCKET_CAN_PORT socketName
const unsigned long TransmitMessages[] = { 129038L, 129793L, 129794L, 0 };

#include <NMEA2000_CAN.h>
#include <N2kMessages.h>

#include "JSONAIS.h"

#include "N2KStream.h"

namespace IO {

	void N2KStreamer::Start() {
		strcpy(socketName, dev.c_str());

		NMEA2000.SetProductInformation("00000002", 100, "AIS-catcher NMEA2000 plugin", "0.55", "0.55");
		NMEA2000.SetDeviceInformation(1, 195, 70, 2046);
		NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, 23);
		NMEA2000.EnableForward(false);
		NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
		NMEA2000.ExtendTransmitMessages(TransmitMessages);
		NMEA2000.SetForwardStream(&serStream);

		if (!NMEA2000.Open()) {
			cout << "NMEA2000: Failed to open CAN port" << endl;
		}

		std::cerr << "NMEA2000: Start thread." << std::endl;

		run_thread = std::thread(&N2KStreamer::run, this);
	}

	void N2KStreamer::Stop() {
		std::cerr << "stop thread" << std::endl;
		running = false;
		run_thread.join();
	}

	void N2KStreamer::emptyQueue() {
		while (!queue.empty()) {
			tN2kMsg* N2kMsg = queue.front();
			queue.pop_front();
			NMEA2000.SendMsg(*N2kMsg);
			delete N2kMsg;
		}
	}

	void N2KStreamer::pushQueue(tN2kMsg* N2kMsg) {
		{
			std::lock_guard<std::mutex> lock(mtx);
			queue.push_back(N2kMsg);
		}
		fifo_cond.notify_one();
	}

	void N2KStreamer::run() {

		while (running) {
			NMEA2000.ParseMessages();
			std::unique_lock<std::mutex> lck(mtx);
			fifo_cond.wait_for(lck, std::chrono::seconds(1));
			emptyQueue();
		}

		{
			std::lock_guard<std::mutex> lck(mtx);
			emptyQueue();
		}
	}

	void N2KStreamer::sendType123(const AIS::Message& ais, const JSON::JSON* data) {

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED, turn = 0;
		int heading = HEADING_UNDEFINED, status = 0, second = 0, raim = 0, accuracy = 0/*, maneuver = 0*/;

		for (const JSON::Property& p : data[0].getProperties()) {
			switch (p.Key()) {
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
/*				
			case AIS::KEY_MANEUVER:
				maneuver = p.Get().getInt();
				break;
*/
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

		tN2kMsg* N2kMsg = new tN2kMsg();

		N2kMsg->SetPGN(129038L);
		N2kMsg->Priority = 4;
		N2kMsg->AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg->Add4ByteUInt(ais.mmsi());
		N2kMsg->Add4ByteDouble(lon, 1e-07);
		N2kMsg->Add4ByteDouble(lat, 1e-07);
		N2kMsg->AddByte(second << 2 | raim << 1 | accuracy);
		N2kMsg->Add2ByteUDouble(cog * (2.0f * PI) / 360.0f, 1e-04);
		N2kMsg->Add2ByteUDouble(speed * 1852.0f / 3600.0f / 10.0, 0.01);
		N2kMsg->AddByte(0x00);
		N2kMsg->AddByte(0x00);
		N2kMsg->AddByte((ais.getChannel() == 'A' ? 0 : 1) << 3);
		N2kMsg->Add2ByteUDouble(heading * (2.0f * PI) / 360.0f, 1e-04);
		N2kMsg->Add2ByteDouble(turn * (2.0f * PI) / 360.0f / 60.0f, 3.125E-05);
		N2kMsg->AddByte(0xF0 | status);
		N2kMsg->AddByte(0xff);
		N2kMsg->AddByte(0xff);

		pushQueue(N2kMsg);
	}

	void N2KStreamer::sendType4(const AIS::Message& ais, const JSON::JSON* data) {
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		int raim = 0, accuracy = 0, hour = 0, minute = 0, second = 0, year = 0, month = 0, day = 0, epfd = 0, days = 0;

		for (const JSON::Property& p : data[0].getProperties()) {
			switch (p.Key()) {
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
				epfd = p.Get().getBool() ? 1 : 0;
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
			}
		}

		tN2kMsg* N2kMsg = new tN2kMsg();

		std::time_t now = std::time(nullptr);
		std::tm* utc_tm = std::gmtime(&now);

		if (utc_tm) {
			utc_tm->tm_year = year - 1900;
			utc_tm->tm_mon = month - 1;
			utc_tm->tm_mday = day;
			days = std::mktime(utc_tm) / 86400;
		}

		N2kMsg->SetPGN(129793L);
		N2kMsg->Priority = 4;
		N2kMsg->AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg->Add4ByteUInt(ais.mmsi());

		N2kMsg->Add4ByteDouble(lon, 1e-07);
		N2kMsg->Add4ByteDouble(lat, 1e-07);

		N2kMsg->AddByte(raim << 1 | accuracy);
		N2kMsg->Add4ByteUInt((hour * 3600 + minute * 60 + second) * 10000);

		N2kMsg->AddByte(0x00);
		N2kMsg->AddByte(0x00);
		N2kMsg->AddByte((ais.getChannel() == 'A' ? 0 : 1) << 3);

		N2kMsg->Add2ByteUInt(days);
		N2kMsg->AddByte(0xf | epfd);
		N2kMsg->AddByte(0xff);

		pushQueue(N2kMsg);
	}

	void N2KStreamer::sendType5(const AIS::Message& ais, const JSON::JSON* data) {

		int ais_version = 0, IMO = 0, shiptype = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, month = 0, day = 0, hour = 0;
		int minute = 0, epfd = 0, dte = 0, nDays = 0;
		char callsign[7] = { ' ' }, shipname[20] = { ' ' }, destination[20] = { ' ' };
		float draught = 0;

		for (const JSON::Property& p : data[0].getProperties()) {
			switch (p.Key()) {
			case AIS::KEY_IMO:
				IMO = p.Get().getInt();
				break;
			case AIS::KEY_CALLSIGN: {
				const std::string& s = p.Get().getString();
				std::memcpy(callsign, s.c_str(), std::min(sizeof(callsign), s.size()));
			} break;
			case AIS::KEY_SHIPNAME: {
				const std::string& s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			} break;
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
			case AIS::KEY_DRAUGHT:
				draught = p.Get().getFloat();
				break;
			case AIS::KEY_DESTINATION: {
				const std::string& s = p.Get().getString();
				std::memcpy(destination, s.c_str(), std::min(sizeof(destination), s.size()));
			} break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			}
		}

		tN2kMsg* N2kMsg = new tN2kMsg();

		if (month && day) {
			std::time_t now = std::time(nullptr);
			std::tm* utc_tm = std::gmtime(&now);

			if (utc_tm) {
				utc_tm->tm_year += (utc_tm->tm_mon + 1 < month || (utc_tm->tm_mon + 1 == month && day < utc_tm->tm_mday)) ? 1 : 0;
				utc_tm->tm_mon = month - 1;
				utc_tm->tm_mday = day;
				nDays = std::mktime(utc_tm) / 86400;
			}
		}

		SetN2kAISClassAStatic(*N2kMsg, ais.type(), (tN2kAISRepeat)ais.repeat(), ais.mmsi(),
							  IMO, callsign, shipname, shiptype, to_bow + to_stern,
							  to_port + to_starboard, to_starboard, to_bow, nDays,
							  (hour * 3600) + (minute * 60), draught / 10.0, destination,
							  (tN2kAISVersion)ais_version, (tN2kGNSStype)epfd,
							  (tN2kAISDTE)dte, (tN2kAISTransceiverInformation)(ais.getChannel() == 'A' ? 0 : 1));

		pushQueue(N2kMsg);
	}


	void N2KStreamer::Receive(const JSON::JSON* data, int ln, TAG& tag) {
		AIS::Message& ais = *((AIS::Message*)data[0].binary);

		if (filter.include(ais)) {

			switch (ais.type()) {
			case 1:
			case 2:
			case 3:
				sendType123(ais, data);
				break;
			case 4:
				sendType4(ais, data);
				break;
			case 5:
				sendType5(ais, data);
				break;
			default:
				break;
			}
			return;
		}
	}

	Setting& N2KStreamer::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "GROUPS_IN") {
			StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "DEVICE") {
			dev = arg;
		}
		else if (!filter.SetOption(option, arg)) {
			throw std::runtime_error("JSON output - unknown option: " + option);
		}
		return *this;
	}
}

#endif