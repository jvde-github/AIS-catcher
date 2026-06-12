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

#include "NMEA.h"
#include "Parse.h"
#include "Convert.h"
#include "Helper.h"
#include "SWAR.h"
#include <cmath>

namespace AIS
{

	void NMEA::reset()
	{
		state = ParseState::FIND_START;
		line.clear();
	}

	void NMEA::clean(const AIVDM &ref)
	{
		auto i = queue.begin();
		uint64_t now = (uint64_t)rxtime_cache;
		while (i != queue.end())
		{
			if ((ref.match_key & 0xFFFFFF00) == (i->match_key & 0xFFFFFF00) || (i->timestamp + 3 < now))
				i = queue.erase(i);
			else
				i++;
		}
	}

	int NMEA::search()
	{
		for (auto it = queue.rbegin(); it != queue.rend(); it++)
			if (matches(aivdm, *it))
				return it->number;
		return 0;
	}

	void NMEA::initMsg(char channel, int src, int64_t toa, long start_idx, long end_idx)
	{
		msg.clear();
		msg.setRxTimeUnix(rxtime_cache);
		msg.setTOA(toa);
		msg.setOrigin(channel, src, own_mmsi);
		msg.setStartIdx(start_idx);
		msg.setEndIdx(end_idx);
	}

	void NMEA::setStamp(bool)
	{
		static bool warned = false;
		if (!warned)
		{
			Warning() << "NMEA: 'stamp' setting is deprecated and ignored — rxtime is always stamped on receive; upstream time is captured in 'toa'";
			warned = true;
		}
	}

	void NMEA::assembleAIS(TAG &tag)
	{
		int src = (mctx.station == -1) ? station : mctx.station;

		// Multi-part: check sequence continuity
		int result = search();
		if (aivdm.number != result + 1)
		{
			clean(aivdm);
			if (aivdm.number != 1)
				return;
		}

		if (queue.size() >= 64)
			queue.erase(queue.begin());
		queue.push_back(aivdm);

		if (aivdm.number != aivdm.count)
			return;

		// All parts collected — assemble and send
		initMsg(aivdm.channel(), src, mctx.toa);

		int parts = 0;
		msg.beginNMEA();
		for (auto &it : queue)
		{
			if (matches(aivdm, it))
			{
				tag.error |= it.message_error;
				addline(it);
				if (!cfg_regenerate)
					msg.pushNMEA(it.sentence);
				parts++;
			}
		}

		if (parts == aivdm.count && msg.validate())
		{
			if (cfg_regenerate)
				msg.buildNMEA(tag, aivdm.match_key & 0x0F);
			Send(&msg, 1, tag);
		}
		else
			warnAIS(WARN_INVALID_MSG, "invalid message", aivdm.sentence);

		clean(aivdm);
	}

	void NMEA::addline(const AIVDM &a)
	{
		msg.appendPayload(a.sentence.data() + a.data_offset, a.data_len);
		if (a.count == a.number)
			msg.reduceLength(a.fillbits);
	}

	void NMEA::split(const std::string &s, size_t offset /*= 0*/)
	{
		splitStr = &s;
		splitCount = 0;
		splitDelim[0] = (int)offset - 1; // sentinel before first field

		const char *ptr = s.data() + offset + 1; // +1 to skip leading $/!
		const char *end = s.data() + s.size();

		// Phase 1: scan size_t words for '*' to find checksum boundary
		// and accumulate XOR checksum, while recording comma positions
		size_t cs_accum = 0;
		const char *p = ptr;
		constexpr size_t mask_star = SWAR::mask('*');
		constexpr size_t mask_comma = SWAR::mask(',');

		// Process size_t-aligned chunks
		while (p + sizeof(size_t) <= end && splitCount < 16)
		{
			size_t word;
			memcpy(&word, p, sizeof(size_t));

			// Check for '*' in this word
			if (SWAR::has_byte(word, mask_star))
				break;

			// Accumulate checksum (all bytes before '*')
			cs_accum ^= word;

			// Check for commas in this word
			if (SWAR::has_byte(word, mask_comma))
			{
				for (size_t j = 0; j < sizeof(size_t) && splitCount < 16; j++)
				{
					if (p[j] == ',')
						splitDelim[++splitCount] = (int)(p - s.data() + j);
				}
			}

			p += sizeof(size_t);
		}

		// Fold the size_t XOR accumulator down to a single byte
		int cs = 0;
		for (size_t k = 0; k < sizeof(size_t); k++)
			cs ^= (int)((cs_accum >> (k * 8)) & 0xFF);

		// Byte-at-a-time tail (remainder + everything after the break)
		bool past_star = false;
		for (; p < end && splitCount < 16; p++)
		{
			char c = *p;
			if (c == '*')
			{
				past_star = true;
				continue;
			}
			if (!past_star)
				cs ^= c;
			if (c == ',')
				splitDelim[++splitCount] = (int)(p - s.data());
		}

		splitChecksum = cs;
		splitDelim[++splitCount] = (int)s.size(); // sentinel after last field
	}

	static bool parse_uint(const char *p, int len, int64_t &out)
	{
		if (len <= 0)
			return false;
		int64_t v = 0;
		for (int i = 0; i < len; i++)
		{
			if (p[i] < '0' || p[i] > '9')
				return false;
			v = v * 10 + (p[i] - '0');
		}
		out = v;
		return true;
	}

	int NMEA::partInt(int i) const
	{
		const char *p = partPtr(i);
		int len = partLen(i);
		bool neg = (len > 0 && p[0] == '-');
		int64_t v;
		if (!parse_uint(p + neg, len - neg, v))
			return 0;
		return neg ? -(int)v : (int)v;
	}

	// stackoverflow variant (need to find the reference)
	float NMEA::GpsToDecimal(const char *nmeaPos, int nmeaLen, char quadrant, bool &error)
	{
		float v = 0;
		if (nmeaPos && nmeaLen > 5)
		{
			char integerPart[3 + 1];
			int digitCount = (nmeaPos[4] == '.' ? 2 : 3);
			memcpy(integerPart, nmeaPos, digitCount);
			integerPart[digitCount] = 0;
			nmeaPos += digitCount;

			int degrees = atoi(integerPart);
			if (degrees == 0 && integerPart[0] != '0')
			{
				error |= true;
				return 0;
			}

			float minutes = (float)atof(nmeaPos);
			if (minutes == 0 && nmeaPos[0] != '0')
			{
				error |= true;
				return 0;
			}

			v = degrees + minutes / 60.0;
			if (quadrant == 'W' || quadrant == 'S')
				v = -v;
		}
		return v;
	}

	bool NMEA::processGPS(const std::string &s, TAG &tag, const char *name,
						  int min_fields, int max_fields,
						  int lat_idx, int ns_idx, int lon_idx, int ew_idx,
						  int fix_idx, int status_idx)
	{
		if (!cfg_GPS)
			return true;

		split(s);

		if (splitCount < min_fields || splitCount > max_fields)
		{
			if (shouldWarn(WARN_GPS_FIELD_COUNT))
				Warning() << name << ": expected " << min_fields << " fields, got " << splitCount << " [" << s << "]";
			return false;
		}

		int last = splitCount - 1;
		int checksum = partLen(last) > 2 ? (hexDigitValue(partAt(last, partLen(last) - 2)) << 4) | hexDigitValue(partAt(last, partLen(last) - 1)) : -1;
		if (checksum != splitChecksum)
		{
			if (shouldWarn(WARN_GPS_CHECKSUM))
				Warning() << name << ": checksum mismatch [" << s << "]";
			if (cfg_crc_check)
				return false;
		}

		// GGA fix quality: accept 1 (GPS), 2 (DGPS), 3 (PPS), 4 (RTK fixed),
		// 5 (RTK float); reject 0 (invalid), 6 (dead reckoning), 7 (manual),
		// 8 (simulation) — those are not measured positions.
		if (fix_idx >= 0)
		{
			int fix = partInt(fix_idx);
			if (fix < 1 || fix > 5)
				return false;
		}

		// A/V status flag (RMC/GLL): a GNSS without a fix still outputs its
		// last stored position flagged 'V' — must not be applied.
		if (status_idx >= 0 && (partEmpty(status_idx) || partAt(status_idx, 0) != 'A'))
			return false;

		if (partEmpty(ns_idx) || partEmpty(ew_idx))
			return false;

		bool error = false;
		float lat = GpsToDecimal(partPtr(lat_idx), partLen(lat_idx), partAt(ns_idx, 0), error);
		float lon = GpsToDecimal(partPtr(lon_idx), partLen(lon_idx), partAt(ew_idx, 0), error);

		if (error)
		{
			if (shouldWarn(WARN_GPS_COORDS))
				Warning() << name << ": invalid coordinates [" << s << "]";
			return false;
		}

		GPS gps(lat, lon, s, false);
		outGPS.Send(&gps, 1, tag);
		return true;
	}

	bool NMEA::processAIS(const std::string &str, TAG &tag)
	{
		if (str.size() < 18 || (str[0] != '$' && str[0] != '!'))
			return false;

		const char *p = str.data();
		const int len = (int)str.size();

		if (memcmp(p + 3, "VDM", 3) != 0 && memcmp(p + 3, "VDO", 3) != 0)
			return true;

		char t1 = p[1], t2 = p[2];
		if (!(t1 >= 'A' && t1 <= 'Z') || (!(t2 >= 'A' && t2 <= 'Z') && !(t2 >= '0' && t2 <= '9')))
		{
			warnAIS(WARN_AIS_TALKER, "invalid talker ID", str);
			return false;
		}

		if (p[6] != ',' || p[8] != ',' || p[10] != ',' || p[7] < '1' || p[7] > '9' || p[9] < '1' || p[9] > '9')
		{
			warnAIS(WARN_AIS_HEADER, "malformed header", str);
			return false;
		}

		// Parse from back: ,F*HH
		const char *end = p + len;
		if (!isHexDigit(end[-1]) || !isHexDigit(end[-2]) || end[-3] != '*' || end[-5] != ',')
		{
			warnAIS(WARN_AIS_TAIL, "malformed tail", str);
			return false;
		}
		int checksum = (hexDigitValue(end[-2]) << 4) | hexDigitValue(end[-1]);
		char fillbits_ch = end[-4];
		if (fillbits_ch < '0' || fillbits_ch > '5')
		{
			warnAIS(WARN_AIS_FILLBITS, "invalid fillbits", str);
			return false;
		}

		// Fields 3 (ID) and 4 (channel): 0 or 1 char each
		const char *q = p + 11;
		char id_ch = 0;
		if (*q != ',')
			id_ch = *q++;
		if (*q != ',')
		{
			warnAIS(WARN_AIS_ID, "invalid ID field", str);
			return false;
		}
		q++;

		char channel_ch = '?';
		if (*q != ',')
			channel_ch = *q++;
		if (*q != ',')
		{
			warnAIS(WARN_AIS_CHANNEL, "invalid channel field", str);
			return false;
		}
		q++;

		const char *payload_end = end - 5;
		if (q > payload_end)
		{
			warnAIS(WARN_AIS_PAYLOAD, "empty payload", str);
			return false;
		}

		// Valid AIS payload bytes: [0x30..0x57] or [0x60..0x77]. Bitwise | (not ||)
		// keeps this branchless so GCC can auto-vectorize.
		for (const char *r = q; r < payload_end; r++)
		{
			unsigned c = (unsigned char)*r;
			unsigned ok = ((c - '0') < 0x28u) | ((c - '`') < 0x18u);
			if (!ok)
			{
				if (c == ',')
					warnAIS(WARN_AIS_PAYLOAD, "too many parts", str);
				else
					warnAIS(WARN_AIS_PAYLOAD, "invalid payload character", str);
				return false;
			}
		}

		// SWAR XOR checksum over [p+1, end-3).
		size_t cs_acc = 0;
		const char *r = p + 1;
		const char *cs_end = end - 3;
		while (r + sizeof(size_t) <= cs_end)
		{
			size_t w;
			memcpy(&w, r, sizeof(w));
			cs_acc ^= w;
			r += sizeof(size_t);
		}

#if SIZE_MAX > 0xFFFFFFFFu
		cs_acc ^= cs_acc >> 32;
#endif
		cs_acc ^= cs_acc >> 16;
		cs_acc ^= cs_acc >> 8;
		int cs = (int)(cs_acc & 0xFF);
		while (r < cs_end)
			cs ^= *r++;

		int src = (mctx.station == -1) ? station : mctx.station;
		int fillbits = fillbits_ch - '0';
		uint32_t message_error = 0;

		if (checksum != cs)
		{
			warnAIS(WARN_NMEA_CHECKSUM, "checksum mismatch", str);
			if (cfg_crc_check)
				return true;
			message_error = MESSAGE_ERROR_NMEA_CHECKSUM;
		}

		// Single-part: decode and send directly, no aivdm construction
		if (p[7] == '1')
		{
			tag.error = message_error;

			initMsg(channel_ch, src, mctx.toa, mctx.ssc, mctx.ssc + mctx.sl);
			msg.appendPayload(q, (int)(payload_end - q));
			msg.reduceLength(fillbits);

			if (msg.validate())
			{
				if (cfg_regenerate)
					msg.buildNMEA(tag);
				else
				{
					msg.beginNMEA();
					msg.pushNMEA(str);
				}
				Send(&msg, 1, tag);
			}
			else
				warnAIS(WARN_INVALID_MSG, "invalid message", str);
			return true;
		}

		// Multi-part: construct aivdm and hand off to fragment assembler
		aivdm.reset(rxtime_cache);
		aivdm.count = p[7] - '0';
		aivdm.number = p[9] - '0';
		uint8_t id = id_ch ? (id_ch - '0') : 0;

		if (mctx.groupId != 0)
			aivdm.match_key = (1u << 31) | (uint32_t)mctx.groupId;
		else
			aivdm.match_key = ((uint32_t)(uint8_t)t1 << 24) | ((uint32_t)(uint8_t)t2 << 16) | ((uint32_t)(uint8_t)channel_ch << 8) | ((uint32_t)aivdm.count << 4) | id;

		aivdm.data_offset = (int)(q - str.data());
		aivdm.data_len = (int)(payload_end - q);
		aivdm.fillbits = fillbits;
		aivdm.message_error = message_error;

		splitChecksum = cs;
		aivdm.sentence = str;

		assembleAIS(tag);
		return true;
	}

	void NMEA::processJSONsentence(TAG &tag)
	{
		tag.clear();
		mctx.reset();

		if (line[0] != '{')
			return;

		try
		{
			parser.parse_into(jsonDoc, line);

			enum
			{
				CLS_UNKNOWN,
				CLS_AIS,
				CLS_TPV,
				CLS_ERROR,
				CLS_WARNING
			} cls = CLS_UNKNOWN;
			enum
			{
				DEV_UNKNOWN,
				DEV_AIS_CATCHER,
				DEV_DAISY_CATCHER
			} dev = DEV_UNKNOWN;

			bool uuid_match = uuid.empty();
			const std::string *message = nullptr;
			const JSON::Value *nmea_array = nullptr;
			float tpv_lat = 0, tpv_lon = 0;
			bool has_tpv_coords = false;
			bool saw_toa = false;

			for (const auto &p : jsonDoc.getMembers())
			{
				switch (p.Key())
				{
				case AIS::KEY_CLASS:
				{
					const auto &s = p.Get().getStringRef();
					if (s == "AIS")
						cls = CLS_AIS;
					else if (s == "TPV")
						cls = CLS_TPV;
					else if (s == "error")
						cls = CLS_ERROR;
					else if (s == "warning")
						cls = CLS_WARNING;
					break;
				}
				case AIS::KEY_UUID:
					if (!uuid.empty())
						uuid_match = (p.Get().getStringRef() == uuid);
					break;
				case AIS::KEY_DEVICE:
				{
					const auto &s = p.Get().getStringRef();
					if (s == "AIS-catcher")
						dev = DEV_AIS_CATCHER;
					else if (s == "dAISy-catcher")
						dev = DEV_DAISY_CATCHER;
					break;
				}
				case AIS::KEY_SIGNAL_POWER:
				case AIS::KEY_DBM:
				case AIS::KEY_RSSI:
					tag.level = p.Get().getFloat(LEVEL_UNDEFINED);
					break;
				case AIS::KEY_PPM:
				case AIS::KEY_FO:
					tag.ppm = p.Get().getFloat(PPM_UNDEFINED);
					break;
				case AIS::KEY_RXUXTIME:
					if (!saw_toa)
						mctx.toa = (int64_t)std::llround(p.Get().getFloat() * 1000000.0);
					break;
				case AIS::KEY_TOA:
					mctx.toa = (int64_t)std::llround(p.Get().getFloat() * 1000000.0);
					saw_toa = true;
					break;
				case AIS::KEY_STATION_ID:
					mctx.station = p.Get().getInt();
					break;
				case AIS::KEY_VERSION:
					tag.version = p.Get().getInt();
					break;
				case AIS::KEY_HARDWARE:
					tag.hardware = p.Get().getString();
					break;
				case AIS::KEY_DRIVER:
					tag.driver = (Type)p.Get().getInt();
					break;
				case AIS::KEY_SAMPLE_START_COUNT:
					mctx.ssc = p.Get().getInt();
					break;
				case AIS::KEY_SAMPLE_LENGTH:
					mctx.sl = p.Get().getInt();
					break;
				case AIS::KEY_IPV4:
					tag.ipv4 = p.Get().getInt();
					break;
				case AIS::KEY_MESSAGE:
					message = &p.Get().getStringRef();
					break;
				case AIS::KEY_NMEA:
					if (p.Get().isArray())
						nmea_array = &p.Get();
					break;
				case AIS::KEY_LAT:
					tpv_lat = p.Get().isFloat() ? p.Get().getFloat() : (p.Get().isInt() ? (float)p.Get().getInt() : 0.0f);
					has_tpv_coords = true;
					break;
				case AIS::KEY_LON:
					tpv_lon = p.Get().isFloat() ? p.Get().getFloat() : (p.Get().isInt() ? (float)p.Get().getInt() : 0.0f);
					has_tpv_coords = true;
					break;
				}
			}

			bool known_dev = (dev == DEV_AIS_CATCHER || dev == DEV_DAISY_CATCHER);

			if (cls == CLS_AIS && known_dev && uuid_match && nmea_array)
			{
				mctx.groupId = 0;
				queue.clear();
				for (const auto &v : nmea_array->getArray())
					processAIS(v.getString(), tag);
			}

			if (known_dev && message && !message->empty())
			{
				auto driver = Util::Parse::DeviceTypeString(tag.driver);
				if (cls == CLS_ERROR)
					Error() << "[" << driver << "]: " << *message;
				else if (cls == CLS_WARNING)
					Warning() << "[" << driver << "]: " << *message;
				else
					Info() << "[" << driver << "]: " << *message;
			}

			if (cls == CLS_TPV && cfg_GPS && has_tpv_coords && (tpv_lat != 0 || tpv_lon != 0))
			{
				GPS gps(tpv_lat, tpv_lon, line, true);
				outGPS.Send(&gps, 1, tag);
			}
		}
		catch (std::exception const &e)
		{
			if (shouldWarn(WARN_JSON_PARSE))
				Warning() << "JSON: " << e.what() << " [" << line << "]";
		}
	}

	bool NMEA::processBinaryPacket(TAG &tag)
	{
		tag.clear();

		size_t idx = 0;
		size_t plen = line.length();

		// Returns 0..255 on success, -1 on EOF, -2 on bad 0xad escape.
		auto getByte = [&]() -> int
		{
			if (idx >= plen)
				return -1;
			uint8_t byte = line[idx++];
			if (byte != 0xad)
				return byte;
			if (idx >= plen)
				return -1;
			switch ((uint8_t)line[idx++])
			{
			case 0xae: return '\n';
			case 0xaf: return '\r';
			case 0xad: return 0xad;
			default:   return -2;
			}
		};

		auto warnFail = [&](int v, const char *ctx)
		{
			if (shouldWarn(WARN_BINARY_SHORT))
				Warning() << "binary: " << (v == -1 ? "packet too short" : "bad escape") << ctx;
		};

		int v;
		if ((v = getByte()) != 0xac)
		{
			if (v < 0) warnFail(v, "");
			else if (shouldWarn(WARN_BINARY_SHORT)) Warning() << "binary: invalid header";
			return false;
		}
		if ((v = getByte()) != 0x00)
		{
			if (v < 0) warnFail(v, "");
			else if (shouldWarn(WARN_BINARY_SHORT)) Warning() << "binary: invalid header";
			return false;
		}

		v = getByte();
		if (v < 0) { warnFail(v, " in flags"); return false; }
		uint8_t flags = (uint8_t)v;

		long long timestamp = 0;
		for (int i = 0; i < 8; i++)
		{
			v = getByte();
			if (v < 0) { warnFail(v, " in timestamp"); return false; }
			timestamp = (timestamp << 8) | v;
		}

		if (flags & 0x01)
		{
			int h = getByte(), l = getByte(), p = getByte();
			if ((h | l | p) < 0) { warnFail(h < 0 ? h : (l < 0 ? l : p), " in signal info"); return false; }
			tag.level = (int16_t)((h << 8) | l) / 10.0f;
			tag.ppm = (int8_t)p / 10.0f;
		}

		int ch = getByte(), lh = getByte(), ll = getByte();
		if ((ch | lh | ll) < 0) { warnFail(ch < 0 ? ch : (lh < 0 ? lh : ll), " in header"); return false; }
		int length_bits = (lh << 8) | ll;

		if (length_bits < 0 || length_bits > MAX_AIS_LENGTH)
		{
			if (shouldWarn(WARN_BINARY_LENGTH))
				Warning() << "binary: invalid message length " << length_bits;
			return false;
		}

		initMsg((char)ch, station, (int64_t)timestamp * 1000000);

		for (int i = 0; i < (length_bits + 7) / 8; i++)
		{
			v = getByte();
			if (v < 0) { warnFail(v, " in payload"); return false; }
			msg.setUint(i * 8, 8, (uint8_t)v);
		}
		msg.setLength(length_bits);

		if (flags & 0x02)
		{
			uint16_t calc_crc = Util::Helper::CRC16((const uint8_t *)line.data(), idx);
			int ch2 = getByte(), cl2 = getByte();
			if ((ch2 | cl2) < 0) { warnFail(ch2 < 0 ? ch2 : cl2, " in CRC"); return false; }
			uint16_t recv_crc = (ch2 << 8) | cl2;
			if (recv_crc != calc_crc)
			{
				if (shouldWarn(WARN_BINARY_CRC))
					Warning() << "binary: CRC mismatch";
				if (cfg_crc_check)
					return false;
			}
		}

		if (!msg.validate())
		{
			if (shouldWarn(WARN_BINARY_VALIDATION))
				Warning() << "binary: message validation failed (type " << msg.type() << ", length " << msg.getLength() << ")";
			return false;
		}
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
		return true;
	}

	bool NMEA::parseTagBlock(const std::string &s, std::string &nmea)
	{
		// Format: \s:source,c:timestamp,g:seq-total-id*checksum\!AIVDM,...
		const char *data = s.data();
		int len = (int)s.size();

		int tagEnd = 1;
		while (tagEnd < len && data[tagEnd] != '\\')
			tagEnd++;
		if (tagEnd >= len)
		{
			if (shouldWarn(WARN_TAG_NO_CLOSE))
				Warning() << "tag block: no closing backslash [" << s << "]";
			return false;
		}

		nmea = s.substr(tagEnd + 1);
		if (nmea.empty())
		{
			if (shouldWarn(WARN_TAG_NO_NMEA))
				Warning() << "tag block: no NMEA sentence after tag block";
			return false;
		}

		const char *p = data + 1;
		const char *pEnd = data + tagEnd;

		// Find checksum '*' from the end
		const char *star = pEnd;
		while (star > p && star[-1] != '*')
			star--;

		const char *contentEnd = pEnd;
		if (star > p)
		{
			star--;
			contentEnd = star;

			if (star + 3 <= pEnd)
			{
				int expected = (hexDigitValue(star[1]) << 4) | hexDigitValue(star[2]);
				int actual = 0;
				for (const char *q = p; q < star; q++)
					actual ^= *q;
				if (expected != actual)
				{
					if (shouldWarn(WARN_TAG_CHECKSUM))
						Warning() << "tag block: checksum mismatch";
					if (cfg_crc_check)
						return false;
				}
			}
		}

		// Parse comma-delimited fields via pointer walk
		const char *field = p;
		while (field < contentEnd)
		{
			const char *fEnd = field;
			while (fEnd < contentEnd && *fEnd != ',')
				fEnd++;

			int fLen = (int)(fEnd - field);
			if (fLen >= 3 && field[1] == ':')
			{
				const char *val = field + 2;
				int vLen = fLen - 2;

				switch (field[0])
				{
				case 's':
				{
					int64_t n;
					if (vLen > 1 && val[0] == 's' && parse_uint(val + 1, vLen - 1, n))
						mctx.station = (int)n;
					break;
				}
				case 'c':
				{
					bool hasDot = false;
					for (int i = 0; i < vLen; i++)
					{
						if (val[i] == '.')
						{
							hasDot = true;
							break;
						}
					}

					if (hasDot)
					{
						char *end;
						double ts = strtod(val, &end);
						if (end > val)
							mctx.toa = (int64_t)std::llround(ts * 1000000.0);
					}
					else
					{
						bool neg = (vLen > 0 && val[0] == '-');
						int64_t raw;
						if (parse_uint(val + neg, vLen - neg, raw))
						{
							if (neg)
								raw = -raw;
							mctx.toa = (raw > 100000000000LL || raw < -100000000000LL) ? raw : (raw * 1000000);
						}
					}
					break;
				}
				case 'g':
				{
					int dash2 = -1;
					for (int i = 0, seen = 0; i < vLen; i++)
					{
						if (val[i] == '-' && ++seen == 2)
						{
							dash2 = i;
							break;
						}
					}
					int64_t n;
					if (dash2 >= 0 && parse_uint(val + dash2 + 1, vLen - dash2 - 1, n))
						mctx.groupId = (int)n;
					break;
				}
				}
			}
			field = fEnd + 1;
		}

		return true;
	}

	bool NMEA::processNMEAline(const std::string &s, TAG &tag)
	{
		if (s.size() <= 5)
			return true;

		const char *id = s.data() + 3;

		if (memcmp(id, "VDM", 3) == 0)
			return processAIS(s, tag);
		if (memcmp(id, "VDO", 3) == 0 && cfg_VDO)
			return processAIS(s, tag);
		if (memcmp(id, "GGA", 3) == 0)
			return processGPS(s, tag, "GGA", 15, 15, 2, 3, 4, 5, 6);
		if (memcmp(id, "RMC", 3) == 0)
			return processGPS(s, tag, "RMC", 12, 14, 3, 4, 5, 6, -1, 2);
		if (memcmp(id, "GLL", 3) == 0)
			return processGPS(s, tag, "GLL", 8, 8, 1, 2, 3, 4, -1, 6);

		return true;
	}

	bool NMEA::processTagBlock(const std::string &s, TAG &tag)
	{
		tag.clear();
		mctx.reset();

		std::string nmea;

		if (!parseTagBlock(s, nmea))
			return false;

		return processNMEAline(nmea, tag);
	}

	void NMEA::findStart()
	{
		while (pos < bufsize && (unsigned char)buf[pos] <= ' ')
			pos++;
		if (pos >= bufsize)
			return;

		char c = buf[pos];
		switch (c)
		{
		case '{':
			state = ParseState::JSON;
			count = 1;
			break;
		case '\\':
			state = ParseState::TAG_BLOCK;
			break;
		case '$':
		case '!':
			state = ParseState::NMEA;
			break;
		case (char)0xac:
			state = ParseState::BINARY;
			break;
		default:
			state = ParseState::SKIP_TO_EOL;
			return;
		}
		line += c;
		pos++;
	}

	void NMEA::scanLine(TAG &tag)
	{
		int limit = MIN(bufsize, pos + (int)(MAX_NMEA_LINE + 1 - line.size()));
		int start = pos;

		// NMEA sentences and tag blocks contain no whitespace; break on any byte <= ' '.
		SWAR_SKIP_AT(buf, pos, limit, SWAR::has_byte_lt(word, 0x21));
		while (pos < limit && (unsigned char)buf[pos] > ' ')
			pos++;

		if (pos > start)
			line.append(buf + start, pos - start);

		if (pos < bufsize && pos < limit)
		{
			pos++; // skip terminator

			if (state == ParseState::NMEA)
			{
				tag.clear();
				mctx.reset();
				processNMEAline(line, tag);
			}
			else
			{
				processTagBlock(line, tag);
			}
			reset();
			return;
		}

		if ((int)line.size() > MAX_NMEA_LINE)
		{
			state = ParseState::SKIP_TO_EOL;
			line.clear();
		}
	}

	AISC_COLD_NOINLINE void NMEA::warnAIS(int bit, const char *msg, const std::string &ctx)
	{
		if (shouldWarn(bit))
			Warning() << "AIS: " << msg << " [" << ctx << "]";
	}

	AISC_COLD_NOINLINE void NMEA::warnJSONControlChar(char c, const std::string &partial)
	{
		if (shouldWarn(WARN_JSON_NEWLINE))
			Warning() << "NMEA: control char 0x" << std::hex << (int)(unsigned char)c << std::dec
					  << " in uncompleted JSON input [" << partial << "]";
		reset();
	}

	void NMEA::scanJSON(TAG &tag)
	{
		int limit = MIN(bufsize, pos + (int)(MAX_JSON_LINE + 1 - line.size()));
		int start = pos;

		SWAR_SKIP_AT(buf, pos, limit,
					 SWAR::has_byte(word, SWAR::mask('{')) | SWAR::has_byte(word, SWAR::mask('}')) | SWAR::has_byte_lt(word, 0x20));

		while (pos < limit)
		{
			char c = buf[pos++];

			if (c == '{')
			{
				count++;
			}
			else if (c == '}')
			{
				--count;
				if (!count)
				{
					line.append(buf + start, pos - start);
					processJSONsentence(tag);
					reset();
					return;
				}
			}
			else if ((unsigned char)c < 0x20)
			{
				line.append(buf + start, pos - 1 - start);
				warnJSONControlChar(c, line);
				return;
			}
		}

		if ((int)(line.size() + pos - start) > MAX_JSON_LINE)
		{
			state = ParseState::SKIP_TO_EOL;
			line.clear();
			return;
		}

		if (pos > start)
			line.append(buf + start, pos - start);
	}

	void NMEA::scanBinary(TAG &tag)
	{
		int limit = MIN(bufsize, pos + (int)(MAX_BINARY_LINE + 1 - line.size()));
		int start = pos;

		SWAR_SKIP_AT(buf, pos, limit, SWAR::has_byte(word, SWAR::mask('\n')));

		while (pos < limit)
		{
			if (buf[pos] == '\n')
				break;
			pos++;
		}

		if (pos > start)
			line.append(buf + start, pos - start);

		if (pos < bufsize && pos < limit)
		{
			pos++; // skip \n
			processBinaryPacket(tag);
			reset();
			return;
		}

		if ((int)line.size() > MAX_BINARY_LINE)
		{
			state = ParseState::SKIP_TO_EOL;
			line.clear();
		}
	}

	void NMEA::Receive(const RAW *data, int len, TAG &tag)
	{
		std::time(&rxtime_cache);

		for (int j = 0; j < len; j++)
		{
			buf = (const char *)data[j].data;
			bufsize = data[j].size;
			pos = 0;

			while (pos < bufsize)
			{
				switch (state)
				{
				case ParseState::FIND_START:
					findStart();
					break;
				case ParseState::SKIP_TO_EOL:
					while (pos < bufsize && buf[pos] != '\n')
						pos++;
					if (pos < bufsize)
					{
						pos++;
						state = ParseState::FIND_START;
					}
					break;
				case ParseState::NMEA:
				case ParseState::TAG_BLOCK:
					scanLine(tag);
					break;
				case ParseState::JSON:
					scanJSON(tag);
					break;
				case ParseState::BINARY:
					scanBinary(tag);
					break;
				default:
					break;
				}
			}
		}
	}
}
