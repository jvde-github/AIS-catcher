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

#include <iomanip>
#include <cstdint>

#include "Convert.h"
#include "Message.h"
#include "Stream.h"

#include "JSON/JSON.h"
#include "Parser.h"
#include "Writer.h"
#include "Keys.h"

namespace AIS
{

	class NMEA : public SimpleStreamInOut<RAW, Message>
	{
		Message msg;
		int station = 0;
		std::string uuid;

		enum class ParseState
		{
			FIND_START,
			SKIP_TO_EOL,
			NMEA,
			TAG_BLOCK,
			JSON,
			BINARY
		};

		struct AIVDM
		{
			std::string sentence;
			uint64_t timestamp = 0;
			uint32_t match_key = 0; // groupId: (1<<31)|gid, else: (t1<<24)|(t2<<16)|(ch<<8)|(count<<4)|ID
			uint32_t message_error = 0;
			uint16_t data_offset = 0;
			uint16_t data_len = 0;
			uint8_t count = 0;
			uint8_t number = 0;
			uint8_t fillbits = 0;

			void reset(std::time_t rx)
			{
				sentence.clear();
				timestamp = (uint64_t)rx;
				match_key = 0;
				message_error = 0;
				data_offset = 0;
				data_len = 0;
				count = 0;
				number = 0;
				fillbits = 0;
			}

			char channel() const { return (char)((match_key >> 8) & 0xFF); }
		} aivdm;

		static bool matches(const AIVDM &a, const AIVDM &b)
		{
			return a.match_key == b.match_key;
		}

		// Zero-allocation field splitter: stores delimiter positions into source string
		const std::string *splitStr = nullptr;
		int splitDelim[18] = {}; // delimiter positions (leading sentinel + up to 16 commas + trailing sentinel)
		int splitCount = 0; // number of fields
		int splitChecksum = 0; // XOR checksum accumulated during split

		int partLen(int i) const { return splitDelim[i + 1] - splitDelim[i] - 1; }
		char partAt(int i, int j) const { return (*splitStr)[splitDelim[i] + 1 + j]; }
		const char *partPtr(int i) const { return splitStr->data() + splitDelim[i] + 1; }
		bool partEmpty(int i) const { return partLen(i) == 0; }
		std::string partStr(int i) const { return splitStr->substr(splitDelim[i] + 1, partLen(i)); }
		int partInt(int i) const;
		static const int MAX_NMEA_LINE = 1024;
		static const int MAX_JSON_LINE = 2048;
		static const int MAX_BINARY_LINE = 1024;

		ParseState state = ParseState::FIND_START;
		std::string line;
		int count = 0;

		// Scan context — set by Receive(), used by scanners
		const char *buf = nullptr;
		int bufsize = 0;
		int pos = 0;
		// rxtime stamp shared by every msg from one Receive() call.
		std::time_t rxtime_cache = 0;
		// Per-message context — set by scanners/process functions, read by dispatch
		struct MsgCtx {
			int station = -1;
			int groupId = 0;
			uint64_t ssc = 0;
			uint16_t sl = 0;
			int64_t toa = 0;

			void reset() { station = -1; groupId = 0; ssc = 0; sl = 0; toa = 0; }
		} mctx;

		void findStart();
		void scanLine(TAG &tag);
		void scanJSON(TAG &tag);
		void scanBinary(TAG &tag);
		void warnJSONControlChar(char c, const std::string &partial);
		void warnAIS(int bit, const char *msg, const std::string &ctx);
		int own_mmsi = -1;

		std::vector<AIVDM> queue;

		void initMsg(char channel, int src, int64_t toa = 0, long start_idx = 0, long end_idx = 0);
		void assembleAIS(TAG &tag);
		void addline(const AIVDM &a);
		void reset();
		void clean(const AIVDM &ref);
		int search();

		static bool isHexDigit(char c) { return Util::Convert::isHexDigit(c); }
		static int hexDigitValue(char c) { return Util::Convert::hexDigitValue(c); }

		float GpsToDecimal(const char *, int len, char, bool &error);

		bool cfg_regenerate = false;
		bool cfg_crc_check = false;
		bool cfg_JSON_input = false;
		bool cfg_VDO = true;
		bool cfg_warnings = true;
		bool cfg_GPS = true;

		enum WarnBit : int {
			WARN_INVALID_MSG, WARN_GPS_FIELD_COUNT, WARN_GPS_CHECKSUM, WARN_GPS_COORDS,
			WARN_AIS_TALKER, WARN_AIS_HEADER, WARN_AIS_TAIL, WARN_AIS_FILLBITS,
			WARN_AIS_ID, WARN_AIS_CHANNEL, WARN_AIS_PAYLOAD, WARN_NMEA_CHECKSUM,
			WARN_JSON_PARSE, WARN_JSON_NEWLINE,
			WARN_BINARY_SHORT, WARN_BINARY_LENGTH, WARN_BINARY_CRC, WARN_BINARY_VALIDATION,
			WARN_TAG_NO_CLOSE, WARN_TAG_NO_NMEA, WARN_TAG_CHECKSUM
		};
		uint64_t warned = 0;

		bool shouldWarn(int bit) {
			uint64_t m = 1ULL << bit;
			bool first = cfg_warnings && !(warned & m);
			warned |= m; 
			return first;
		}

		JSON::Parser parser;
		JSON::Document jsonDoc;

		void split(const std::string &, size_t offset = 0);
		void processJSONsentence(TAG &tag);
		bool processAIS(const std::string &s, TAG &tag);
		bool processGPS(const std::string &s, TAG &tag, const char *name,
						int min_fields, int max_fields,
						int lat_idx, int ns_idx, int lon_idx, int ew_idx,
						int fix_idx = -1, int status_idx = -1);
		bool processBinaryPacket(TAG &tag);
		bool parseTagBlock(const std::string &s, std::string &nmea);
		bool processTagBlock(const std::string &s, TAG &tag);
		bool processNMEAline(const std::string &s, TAG &tag);

	public:
		NMEA() : parser(JSON_DICT_INPUT)
		{
			parser.setSkipUnknown(true);
		}

		virtual ~NMEA() {}
		void Receive(const RAW *data, int len, TAG &tag);

		void setRegenerate(bool b) { cfg_regenerate = b; }
		bool getRegenerate() { return cfg_regenerate; }

		void setVDO(bool b) { cfg_VDO = b; }
		bool getVDO() { return cfg_VDO; }
		void setUUID(const std::string &u) { uuid = u; }
		const std::string &getUUID() { return uuid; }

		void setStation(int s) { station = s; }
		int getStation() { return station; }

		void setWarnings(bool b) { cfg_warnings = b; }
		void setGPS(bool b) { cfg_GPS = b; }
		void setCRCcheck(bool b) { cfg_crc_check = b; }
		bool getCRCcheck() { return cfg_crc_check; }
		void setJSON(bool b) { cfg_JSON_input = b; }
		void setStamp(bool b);
		void setOwnMMSI(int m) { own_mmsi = m; }

		Connection<GPS> outGPS;
	};
}
