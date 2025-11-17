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

#include <iomanip>

#include "Message.h"
#include "Stream.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/StringBuilder.h"
#include "Keys.h"

namespace AIS
{

	class NMEA : public SimpleStreamInOut<RAW, Message>
	{
		Message msg;
		int station = 0;
		std::string uuid;

		struct AIVDM
		{
			std::string sentence;
			std::string line;
			std::string data;

			uint64_t timestamp;

			void reset()
			{
				sentence.clear();
				data.clear();
				timestamp = time(nullptr);
			}
			char channel;
			int count;
			int number;
			int ID;
			int checksum;
			int fillbits;
			int talkerID;
		} aivdm;

		std::vector<std::string> parts;

		char prev = '\n';
		int state = 0;
		std::string line;
		int count;
		int own_mmsi = -1;

		std::vector<AIVDM> queue;

		void submitAIS(TAG &tag, long int t, uint64_t ssc, uint16_t sl, int thisstation);
		void addline(const AIVDM &a);
		void reset(char);
		void clean(char, int);
		int search(const AIVDM &a);

		bool isNMEAchar(char c) { return (c >= 40 && c < 88) || (c >= 96 && c <= 56 + 0x3F); }
		bool isHEX(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
		int fromHEX(char c) { return (c >= '0' && c <= '9') ? (c - '0') : ((c >= 'A' && c <= 'F') ? (c - 'A' + 10) : (c - 'a' + 10)); }

		int NMEAchecksum(std::string s);

		float GpsToDecimal(const char *, char, bool &error);

		bool regenerate = false;
		bool stamp = true;
		bool crc_check = false;
		bool JSON_input = false;
		bool VDO = true;
		bool warnings = true;
		bool includeGPS = true;

		JSON::Parser parser;

		void split(const std::string &);
		std::string trim(const std::string &);
		void processJSONsentence(const std::string &s, TAG &tag, long t);
		bool processAIS(const std::string &s, TAG &tag, long t, uint64_t ssc, uint16_t sl, int thisstation, std::string &error_msg);
		bool processGGA(const std::string &s, TAG &tag, long t, std::string &error_msg);
		bool processGLL(const std::string &s, TAG &tag, long t, std::string &error_msg);
		bool processRMC(const std::string &s, TAG &tag, long t, std::string &error_msg);
		bool processBinaryPacket(const std::string &packet, TAG &tag, std::string &error_msg);

	public:
		NMEA() : parser(&AIS::KeyMap, JSON_DICT_FULL)
		{
			parser.setSkipUnknown(true);
		}

		virtual ~NMEA() {}
		void Receive(const RAW *data, int len, TAG &tag);

		void setRegenerate(bool b) { regenerate = b; }
		bool getRegenerate() { return regenerate; }

		void setVDO(bool b) { VDO = b; }
		bool getVDO() { return VDO; }
		void setUUID(const std::string &u) { uuid = u; }
		std::string getUUID() { return uuid; }

		void setStation(int s) { station = s; }
		int getStation() { return station; }

		void setWarnings(bool b) { warnings = b; }
		void setGPS(bool b) { includeGPS = b; }
		void setCRCcheck(bool b) { crc_check = b; }
		bool getCRCcheck() { return crc_check; }
		void setJSON(bool b) { JSON_input = b; }
		void setStamp(bool b) { stamp = b; }
		bool getStamp() { return stamp; }
		void setOwnMMSI(int m) { own_mmsi = m; }

		Connection<GPS> outGPS;
	};
}
