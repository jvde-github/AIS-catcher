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
#include <cmath>

namespace AIS
{

	const std::string empty;

	void NMEA::reset(char c)
	{
		state = ParseState::IDLE;
		line.clear();
		prev = c;
	}

	void NMEA::clean(char c, int t, int groupId)
	{
		auto i = queue.begin();
		uint64_t now = time(nullptr);
		while (i != queue.end())
		{
			// Match by groupId if available, otherwise by channel+talkerID
			bool match = (groupId != 0 && i->groupId == groupId) ||
						 (groupId == 0 && i->channel == c && i->talkerID == t);
			if (match || (i->timestamp + 3 < now))
				i = queue.erase(i);
			else
				i++;
		}
	}

	int NMEA::search(const AIVDM &a)
	{
		// multiline message, find previous lines with matching group
		// return: 0 = Not Found, -1: Found but inconsistent with input, >0: number of previous message
		int lastNumber = 0;
		for (auto it = queue.rbegin(); it != queue.rend(); it++)
		{
			// Match by groupId if available, otherwise by channel+talkerID
			bool match = (aivdm.groupId != 0 && it->groupId == aivdm.groupId) ||
						 (aivdm.groupId == 0 && it->channel == aivdm.channel && it->talkerID == aivdm.talkerID);
			if (match)
			{
				if (it->count != aivdm.count || it->ID != aivdm.ID)
					lastNumber = -1;
				else
					lastNumber = it->number;
				break;
			}
		}
		return lastNumber;
	}

	int NMEA::NMEAchecksum(std::string s)
	{
		int c = 0;
		if (s.length() > 4) // Need at least "$X*XX" format
		{
			for (size_t i = 1; i < s.length() - 3; i++)
				c ^= s[i];
		}
		return c;
	}

	void NMEA::submitAIS(TAG &tag, int64_t t, uint64_t ssc, uint16_t sl, int thisstation)
	{
		bool checksum_error = aivdm.checksum != NMEAchecksum(aivdm.sentence);

		if (checksum_error)
		{
			if (warnings)
				Warning() << "NMEA: incorrect checksum [" << aivdm.sentence << "] from station " << (thisstation == -1 ? station : thisstation) << ".";

			if (crc_check)
				return;

			aivdm.message_error |= MESSAGE_ERROR_NMEA_CHECKSUM;
		}

		if (aivdm.count == 1)
		{
			tag.error = aivdm.message_error;

			msg.clear();
			if (stamp || t == 0)
				msg.Stamp();
			else
				msg.setRxTimeMicros(t);
			msg.setOrigin(aivdm.channel, thisstation == -1 ? station : thisstation, own_mmsi);
			msg.setStartIdx(ssc);
			msg.setEndIdx(ssc + sl);

			addline(aivdm);

			if (msg.validate())
			{
				if (regenerate)
					msg.buildNMEA(tag);
				else
					msg.NMEA.push_back(aivdm.sentence);

				Send(&msg, 1, tag);
			}
			else if (msg.getLength() > 0)

				if (warnings)
					Warning() << "NMEA: invalid message of type " << msg.type() << " and length " << msg.getLength() << " from station " << (thisstation == -1 ? station : thisstation) << ".";

			return;
		}

		int result = search(aivdm);

		if (aivdm.number != result + 1 || result == -1)
		{
			clean(aivdm.channel, aivdm.talkerID, aivdm.groupId);
			if (aivdm.number != 1)
			{
				return;
			}
		}

		queue.push_back(aivdm);
		if (aivdm.number != aivdm.count)
			return;

		// multiline messages are now complete and in the right order
		// we create a message and add the payloads to it
		msg.clear();
		if (stamp || t == 0)
			msg.Stamp();
		else
			msg.setRxTimeMicros(t);
		msg.setOrigin(aivdm.channel, thisstation == -1 ? station : thisstation, own_mmsi);

		for (auto it = queue.begin(); it != queue.end(); it++)
		{
			// Match by groupId if available, otherwise by channel+talkerID
			bool match = (aivdm.groupId != 0 && it->groupId == aivdm.groupId) ||
						 (aivdm.groupId == 0 && it->channel == aivdm.channel && it->talkerID == aivdm.talkerID);
			if (match && it->count == aivdm.count && it->ID == aivdm.ID)
			{
				tag.error |= it->message_error;

				addline(*it);
				if (!regenerate)
					msg.NMEA.push_back(it->sentence);
			}
		}

		if (msg.validate())
		{
			if (regenerate)
				msg.buildNMEA(tag, aivdm.ID);

			Send(&msg, 1, tag);
		}
		else if (warnings)
			Warning() << "NMEA: invalid message of type " << msg.type() << " and length " << msg.getLength();

		clean(aivdm.channel, aivdm.talkerID, aivdm.groupId);
	}

	void NMEA::addline(const AIVDM &a)
	{
		for (char d : a.data)
			msg.appendLetter(d);
		if (a.count == a.number)
			msg.reduceLength(a.fillbits);
	}

	void NMEA::split(const std::string &s)
	{
		parts.clear();
		std::stringstream ss(s);
		std::string p;
		while (ss.good())
		{
			getline(ss, p, ',');
			parts.push_back(p);
		}
	}

	std::string NMEA::trim(const std::string &s)
	{
		std::string r;
		for (char c : s)
			if (c != ' ')
				r += c;
		return r;
	}

	// stackoverflow variant (need to find the reference)
	float NMEA::GpsToDecimal(const char *nmeaPos, char quadrant, bool &error)
	{
		float v = 0;
		if (nmeaPos && strlen(nmeaPos) > 5)
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

	bool NMEA::processGGA(const std::string &s, TAG &tag, int64_t t, std::string &error_msg)
	{

		if (!includeGPS)
			return true;

		bool error = false;

		split(s);

		if (parts.size() != 15)
		{
			error_msg = "NMEA: GPGGA does not have 15 parts but " + std::to_string(parts.size());
			return false;
		}

		const std::string &crc = parts[14];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line))
		{
			if (crc_check)
			{

				error_msg = "NMEA: incorrect checksum.";
				return false;
			}
		}

		// no proper fix
		int fix = atoi(parts[6].c_str());
		if (fix != 1 && fix != 2)
		{
			error_msg = "NMEA: no fix in GPGGA NMEA:" + parts[6];
			return false;
		}

		std::string lat_coord = trim(parts[2]);
		std::string lat_quad = trim(parts[3]);
		std::string lon_coord = trim(parts[4]);
		std::string lon_quad = trim(parts[5]);

		if (lat_quad.empty() || lon_quad.empty())
		{
			return false;
		}

		GPS gps(GpsToDecimal(lat_coord.c_str(), lat_quad[0], error),
				GpsToDecimal(lon_coord.c_str(), lon_quad[0], error),
				s, empty);

		if (error)
		{
			error_msg = "NMEA: error in GPGGA coordinates.";
			return false;
		}

		outGPS.Send(&gps, 1, tag);

		return true;
	}

	bool NMEA::processRMC(const std::string &s, TAG &tag, int64_t t, std::string &error_msg)
	{

		if (!includeGPS)
			return true;

		bool error = false;

		split(s);

		if ((parts.size() < 12 || parts.size() > 14))
			return false;

		const std::string &crc = parts[parts.size() - 1];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line))
		{
			if (crc_check)
			{
				error_msg = "incorrect checksum";
				return false;
			}
		}

		std::string lat_quad = trim(parts[4]); // N/S
		std::string lon_quad = trim(parts[6]); // E/W

		if (lat_quad.empty() || lon_quad.empty())
		{
			error_msg = "NMEA: no coordinates in RMC";
			return false;
		}

		GPS gps(GpsToDecimal(trim(parts[3]).c_str(), lat_quad[0], error), // lat
				GpsToDecimal(trim(parts[5]).c_str(), lon_quad[0], error), // lon
				s, empty);

		if (error)
		{
			error_msg = "NMEA: error in RMC coordinates.";
			return false;
		}

		outGPS.Send(&gps, 1, tag);

		return true;
	}

	bool NMEA::processGLL(const std::string &s, TAG &tag, int64_t t, std::string &error_msg)
	{

		if (!includeGPS)
			return true;

		bool error = false;
		split(s);

		if (parts.size() != 8)
		{
			error_msg = "NMEA: GLL does not have 8 parts but " + std::to_string(parts.size());
			return false;
		}

		const std::string &crc = parts[7];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line))
		{
			if (warnings && !crc_check)
				Warning() << "NMEA: incorrect checksum [" << line << "].";

			if (crc_check)
			{
				error_msg = "NMEA: incorrect checksum";
				return false;
			}
		}

		std::string lat_quad = parts[2];
		std::string lon_quad = parts[4];

		if (lat_quad.empty() || lon_quad.empty())
		{
			return false;
		}

		float lat = GpsToDecimal(parts[1].c_str(), lat_quad[0], error);
		float lon = GpsToDecimal(parts[3].c_str(), lon_quad[0], error);

		if (error)
		{
			error_msg = "NMEA: error in GLL coordinates.";
			return false;
		}

		GPS gps(lat, lon, s, empty);
		outGPS.Send(&gps, 1, tag);

		return true;
	}

	bool NMEA::processAIS(const std::string &str, TAG &tag, int64_t t, uint64_t ssc, uint16_t sl, int thisstation, int groupId, std::string &error_msg)
	{
		int pos = str.find_first_of("$!");
		if (pos == std::string::npos)
		{
			error_msg = "NMEA: no $ or ! in AIS sentence";
			return false;
		}
		std::string nmea = str.substr(pos);

		bool isNMEA = nmea.size() > 10 && (nmea[3] == 'V' && nmea[4] == 'D' && (nmea[5] == 'M' || nmea[5] == 'O'));
		if (!isNMEA)
		{
			return true; // no NMEA -> ignore
		}

		split(nmea);
		aivdm.reset();

		if (parts.size() != 7 || parts[0].size() != 6 || parts[1].size() != 1 || parts[2].size() != 1 || parts[3].size() > 1 || parts[4].size() > 1 || parts[6].size() != 4)
		{
			error_msg = "NMEA: AIS sentence does not have 7 parts or has invalid part sizes";
			return false;
		}
		if (parts[0][0] != '$' && parts[0][0] != '!')
		{
			error_msg = "NMEA: AIS sentence does not start with $ or !";
			return false;
		}

		if (!std::isupper(parts[0][1]) || (!std::isupper(parts[0][2]) && !std::isdigit(parts[0][2])))
		{
			error_msg = "NMEA: AIS sentence does not have valid talker ID";
			return false;
		}

		if (parts[0][3] != 'V' || parts[0][4] != 'D' || (parts[0][5] != 'M' && parts[0][5] != 'O'))
		{
			error_msg = "NMEA: AIS sentence does not have valid VDM or VDO";
			return false;
		}

		aivdm.talkerID = ((parts[0][1] << 8) | parts[0][2]);
		aivdm.count = parts[1][0] - '0';
		aivdm.number = parts[2][0] - '0';
		aivdm.ID = parts[3].size() > 0 ? parts[3][0] - '0' : 0;
		aivdm.channel = parts[4].size() > 0 ? parts[4][0] : '?';

		for (auto c : parts[5])
		{
			if (!isNMEAchar(c))
			{
				error_msg = "NMEA: AIS sentence contains invalid NMEA character '" + std::string(1, c) + "'";
				return false;
			}
		}
		aivdm.data = parts[5];
		aivdm.fillbits = parts[6][0] - '0';

		if (!isHEX(parts[6][2]) || !isHEX(parts[6][3]))
		{
			error_msg = "NMEA: AIS sentence does not have valid checksum";
			return false;
		}
		aivdm.checksum = (fromHEX(parts[6][2]) << 4) | fromHEX(parts[6][3]);

		aivdm.sentence = nmea;
		aivdm.groupId = groupId;

		submitAIS(tag, t, ssc, sl, thisstation);

		return true;
	}

	void NMEA::processJSONsentence(const std::string &s, TAG &tag, int64_t t)
	{

		if (s[0] == '{')
		{
			try
			{
				// JSON::Parser parser(&AIS::KeyMap, JSON_DICT_FULL);
				// parser.setSkipUnknown(true);
				std::shared_ptr<JSON::JSON> j = parser.parse(s);

				std::string cls = "";
				std::string dev = "";
				std::string suuid = "";
				std::string message = "";
				int thisstation = -1;
				uint64_t ssc = 0;
				uint16_t sl = 0;

				t = 0;

				// phase 1, get the meta data in place
				for (const auto &p : j->getProperties())
				{
					switch (p.Key())
					{
					case AIS::KEY_CLASS:
						cls = p.Get().getString();
						break;
					case AIS::KEY_UUID:
						suuid = p.Get().getString();
						break;
					case AIS::KEY_DEVICE:
						dev = p.Get().getString();
						break;
					case AIS::KEY_SIGNAL_POWER:
						tag.level = p.Get().getFloat(LEVEL_UNDEFINED);
						break;
					case AIS::KEY_PPM:
						tag.ppm = p.Get().getFloat(PPM_UNDEFINED);
						break;
					case AIS::KEY_RXUXTIME:
					{
						double ts = p.Get().getFloat();
						t = (int64_t)std::llround(ts * 1000000.0);
					}
					break;
					case AIS::KEY_STATION_ID:
						thisstation = p.Get().getInt();
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
						ssc = p.Get().getInt();
						break;
					case AIS::KEY_SAMPLE_LENGTH:
						sl = p.Get().getInt();
						break;
					case AIS::KEY_IPV4:
						tag.ipv4 = p.Get().getInt();
						break;
					case AIS::KEY_MESSAGE:
						message = p.Get().getString();
						break;
					}
				}

				if (cls == "AIS" && dev == "AIS-catcher" && (uuid.empty() || suuid == uuid))
				{

					for (const auto &p : j->getProperties())
					{
						if (p.Key() == AIS::KEY_NMEA)
						{
							if (p.Get().isArray())
							{
								for (const auto &v : p.Get().getArray())
								{
									std::string error;
									const std::string &line = v.getString();

									if (!processAIS(line, tag, t, ssc, sl, thisstation, 0, error))
									{
										Warning() << "NMEA [" << (tag.ipv4 ? (Util::Convert::IPV4toString(tag.ipv4) + " - ") : "") << thisstation << "] " << error << " (" << line << ")";
									}
								}
							}
						}
					}
				}

				if (dev == "AIS-catcher" && !message.empty())
				{
					if (cls == "error")
					{

						Error() << "[" << Util::Parse::DeviceTypeString(tag.driver) << "]: " << message;
					}
					else if (cls == "warning")
					{
						Warning() << "[" << Util::Parse::DeviceTypeString(tag.driver) << "]: " << message;
					}
					else
					{
						Info() << "[" << Util::Parse::DeviceTypeString(tag.driver) << "]: " << message;
					}
				}

				if (cls == "TPV" && includeGPS)
				{
					float lat = 0, lon = 0;

					for (const auto &p : j->getProperties())
					{
						if (p.Key() == AIS::KEY_LAT)
						{
							if (p.Get().isFloat())
							{
								lat = p.Get().getFloat();
							}
							else if (p.Get().isInt())
							{
								lat = (float)p.Get().getInt();
							}
						}
						else if (p.Key() == AIS::KEY_LON)
						{
							if (p.Get().isFloat())
							{
								lon = p.Get().getFloat();
							}
							else if (p.Get().isInt())
							{
								lon = (float)p.Get().getInt();
							}
						}
					}

					if (lat != 0 || lon != 0)
					{
						GPS gps(lat, lon, empty, s);
						outGPS.Send(&gps, 1, tag);
					}
				}
			}
			catch (std::exception const &e)
			{
				Error() << "NMEA model: " << e.what();
			}
		}
	}

	bool NMEA::processBinaryPacket(const std::string &packet, TAG &tag, std::string &error_msg)
	{
		try
		{
			size_t idx = 0;
			auto getByte = [&]() -> uint8_t
			{
				if (idx >= packet.length())
					throw std::runtime_error("packet too short");

				uint8_t byte = packet[idx++];

				if (byte == 0xad)
				{
					if (idx >= packet.length())
						throw std::runtime_error("packet too short");

					uint8_t next = packet[idx++];

					if (next == 0xae)
						return '\n';
					if (next == 0xaf)
						return '\r';
					if (next == 0xad)
						return 0xad;
					throw std::runtime_error("invalid escape sequence");
				}
				return byte;
			};

			if (getByte() != 0xac)
				throw std::runtime_error("invalid magic byte");
			if (getByte() != 0x00)
				throw std::runtime_error("unsupported version");

			uint8_t flags = getByte();

			long long timestamp = 0;

			for (int i = 0; i < 8; i++)
				timestamp = (timestamp << 8) | getByte();

			if (flags & 0x01)
			{
				tag.level = (int16_t)((getByte() << 8) | getByte()) / 10.0f;
				tag.ppm = (int8_t)getByte() / 10.0f;
			}

			char channel = getByte();
			int length_bits = (getByte() << 8) | getByte();

			if (length_bits < 0 || length_bits > MAX_AIS_LENGTH)
			{
				throw std::runtime_error("invalid message length: " + std::to_string(length_bits));
			}

			msg.clear();
			msg.setRxTimeUnix(timestamp);
			msg.setOrigin(channel, station, own_mmsi);
			msg.setStartIdx(0);
			msg.setEndIdx(0);

			for (int i = 0; i < (length_bits + 7) / 8; i++)
				msg.setUint(i * 8, 8, getByte());

			msg.setLength(length_bits);

			if (flags & 0x02)
			{
				uint16_t calc_crc = Util::Helper::CRC16((const uint8_t *)packet.data(), idx);
				uint16_t recv_crc = (getByte() << 8) | getByte();
				if (recv_crc != calc_crc)
				{
					if (crc_check)
						throw std::runtime_error("CRC mismatch");
					if (warnings)
						Warning() << "NMEA: binary packet CRC mismatch";
				}
			}

			if (!msg.validate())
			{
				error_msg = "message validation failed";
				return false;
			}
			msg.buildNMEA(tag);
			Send(&msg, 1, tag);
			return true;
		}
		catch (const std::exception &e)
		{
			error_msg = e.what();
			return false;
		}
	}

	bool NMEA::parseTagBlock(const std::string &s, std::string &nmea, int64_t &timestamp, int &thisstation, int &groupId, std::string &error_msg)
	{
		// Format: \s:source,c:timestamp,g:seq-total-id*checksum\!AIVDM,...
		// Find the second backslash that ends the tag block
		size_t tagEnd = s.find('\\', 1);
		if (tagEnd == std::string::npos)
		{
			error_msg = "tag block: no closing backslash";
			return false;
		}

		std::string tagBlock = s.substr(1, tagEnd - 1); // Extract content between backslashes
		nmea = s.substr(tagEnd + 1);					// NMEA sentence after tag block

		if (nmea.empty())
		{
			error_msg = "tag block: no NMEA sentence after tag block";
			return false;
		}

		// Remove checksum from tag block if present
		size_t checksumPos = tagBlock.find('*');
		if (checksumPos != std::string::npos)
		{
			// Verify checksum - need at least 2 hex chars after '*'
			int expectedChecksum = 0;
			if (checksumPos + 2 < tagBlock.size())
			{
				expectedChecksum = (fromHEX(tagBlock[checksumPos + 1]) << 4) | fromHEX(tagBlock[checksumPos + 2]);
			}
			int actualChecksum = 0;
			for (size_t i = 0; i < checksumPos; i++)
				actualChecksum ^= tagBlock[i];

			if (expectedChecksum != actualChecksum && crc_check)
			{
				error_msg = "tag block: checksum mismatch";
				return false;
			}
			tagBlock = tagBlock.substr(0, checksumPos);
		}

		// Parse individual fields
		std::stringstream ss(tagBlock);
		std::string field;
		while (std::getline(ss, field, ','))
		{
			if (field.size() < 2 || field[1] != ':')
				continue;

			char key = field[0];
			std::string value = field.substr(2);

			switch (key)
			{
			case 's': // Source - if starts with 's' followed by digits, extract station ID
				if (!value.empty() && value[0] == 's')
				{
					try
					{
						thisstation = std::stoi(value.substr(1));
					}
					catch (...)
					{
					}
				}
				break;
			case 'c': // Unix timestamp
				try
				{
					if (value.find('.') != std::string::npos)
					{
						double ts = std::stod(value);
						timestamp = (int64_t)std::llround(ts * 1000000.0);
					}
					else
					{
						int64_t raw = std::stoll(value);
						timestamp = (raw > 100000000000LL || raw < -100000000000LL) ? raw : (raw * 1000000);
					}
				}
				catch (...)
				{
				}
				break;
			case 'g': // Group: seq-total-groupId (e.g., "1-2-1234")
			{
				size_t dash1 = value.find('-');
				size_t dash2 = value.find('-', dash1 + 1);
				if (dash1 != std::string::npos && dash2 != std::string::npos)
				{
					try
					{
						groupId = std::stoi(value.substr(dash2 + 1));
					}
					catch (...)
					{
					}
				}
			}
			break;
			}
		}

		return true;
	}

	bool NMEA::isCompleteNMEA(const std::string &s, bool newline)
	{
		if (s.size() < 7)
			return false;

		// Check for VDM/VDO with valid checksum pattern
		bool isVDx = s.size() > 10 && (s[3] == 'V' && s[4] == 'D' && (s[5] == 'M' || s[5] == 'O'));
		if (isVDx)
		{
			bool hasChecksum = isHEX(s[s.size() - 1]) && isHEX(s[s.size() - 2]) && s[s.size() - 3] == '*' &&
							   ((isdigit(s[s.size() - 4]) && s[s.size() - 5] == ',') || (s[s.size() - 4] == ','));
			if (hasChecksum)
				return true;
		}

		// For other NMEA types or incomplete VDM/VDO, require newline
		return newline;
	}

	bool NMEA::processNMEAline(const std::string &s, TAG &tag, int64_t t, int thisstation, int groupId, std::string &error_msg)
	{
		std::string type = s.size() > 5 ? s.substr(3, 3) : "";

		if (type == "VDM")
			return processAIS(s, tag, t, 0, 0, thisstation, groupId, error_msg);
		if (type == "VDO" && VDO)
			return processAIS(s, tag, t, 0, 0, thisstation, groupId, error_msg);
		if (type == "GGA")
			return processGGA(s, tag, t, error_msg);
		if (type == "RMC")
			return processRMC(s, tag, t, error_msg);
		if (type == "GLL")
			return processGLL(s, tag, t, error_msg);

		return true; // Unknown type, ignore
	}

	bool NMEA::processTagBlock(const std::string &s, TAG &tag, int64_t &t, std::string &error_msg)
	{
		std::string nmea;
		int thisstation = -1;
		int groupId = 0;

		if (!parseTagBlock(s, nmea, t, thisstation, groupId, error_msg))
			return false;

		return processNMEAline(nmea, tag, t, thisstation, groupId, error_msg);
	}

	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW *data, int len, TAG &tag)
	{
		try
		{
			int64_t t = 0;

			for (int j = 0; j < len; j++)
			{
				for (int i = 0; i < data[j].size; i++)
				{
					char c = ((char *)(data[j].data))[i];

					if (state == ParseState::IDLE)
					{
						if (c == '{' && (prev == '\n' || prev == '\r' || prev == '}'))
						{
							line = c;
							state = ParseState::JSON;
							count = 1;
						}
						else if (c == '\\' && (prev == '\n' || prev == '\r'))
						{
							line = c;
							state = ParseState::TAG_BLOCK;
						}
						else if (c == '$' || c == '!')
						{
							line = c;
							state = ParseState::NMEA;
						}
						else if ((unsigned char)c == 0xac)
						{
							line = c;
							state = ParseState::BINARY;
						}
						prev = c;
						continue;
					}

					bool newline = (state == ParseState::BINARY) ? (c == '\n') : (c == '\r' || c == '\n' || c == '\t' || c == '\0');

					if (!newline)
						line += c;
					prev = c;
					if (state == ParseState::JSON)
					{
						// we do not allow nested JSON, so processing until newline character or '}'
						if (c == '{')
							count++;
						else if (c == '}')
						{
							--count;
							if (!count)
							{
								t = 0;
								tag.clear();

								processJSONsentence(line, tag, t);
								reset(c);
							}
						}
						else if (newline)
						{
							if (warnings)
								Warning() << "NMEA: newline in uncompleted JSON input not allowed";
							reset(c);
						}
					}
					else if (state == ParseState::NMEA)
					{
						if (isCompleteNMEA(line, newline))
						{
							std::string error = "unspecified error";
							tag.clear();
							t = 0;

							if (!processNMEAline(line, tag, t, -1, 0, error))
							{
								if (warnings)
								{
									Warning() << "NMEA: error processing NMEA line " << line;
									Warning() << "NMEA [" << error << " (" << line << ")";
								}
							}
							reset(c);
						}
					}
					else if (state == ParseState::BINARY)
					{
						// Binary packet processing - collect until newline
						if (c == '\n')
						{
							std::string error;
							tag.clear();
							if (!processBinaryPacket(line, tag, error))
							{
								if (warnings)
								{
									std::stringstream ss;
									ss << "NMEA: error processing binary packet: " << error
									   << " (length: " << line.length() << " bytes)";
									Warning() << ss.str();
								}
							}
							reset(c);
						}
					}
					else if (state == ParseState::TAG_BLOCK)
					{
						// Tag block processing - format: \s:source,c:timestamp,g:seq-total-id*checksum\NMEA
						// Find NMEA part after tag block to check for completion
						size_t tagEnd = line.find('\\', 1);
						std::string nmeaPart = (tagEnd != std::string::npos) ? line.substr(tagEnd + 1) : "";
						if (isCompleteNMEA(nmeaPart, newline))
						{
							// Parse complete tag block line
							std::string error;
							tag.clear();
							t = 0;

							if (!processTagBlock(line, tag, t, error))
							{
								if (warnings)
									Warning() << "NMEA: " << error;
							}
							reset(c);
						}
					}
					if (line.size() > 1024)
						reset(c);
				}
			}
		}
		catch (std::exception &e)
		{
			if (warnings)
				Warning() << "NMEA Receive: " << e.what();
		}
	}
}
