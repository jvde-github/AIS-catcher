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
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdio>

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "Parse.h"
#include "Convert.h"
#include "Helper.h"
#include "ADSB.h"
#include "Keys.h"
#include "OutputStats.h"
#include "JSON/JSON.h"
#include "JSON/Writer.h"
#include "Logger.h"

class Receiver;

namespace IO
{
	class OutputMessage : public StreamIn<AIS::Message>, public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public Setting
	{
	protected:
		std::string json;
		AIS::Filter filter;
		JSON::Serializer builder{JSON_DICT_FULL};

		OutputStats stats;
		std::string description, link, type;
		uint64_t hub_lines = 0;

		std::string uuid;
		bool include_sample_start = false;
		bool forward_gps = true;
		const char *line_suffix = "\r\n";

		// Formats one AIS message into the reusable `json` member buffer.
		// Zero-allocation in steady state: clear() preserves capacity.
		void formatInto(const AIS::Message &msg, TAG &tag)
		{
			json.clear();
			switch (fmt)
			{
			case MessageFormat::NMEA:
				for (const auto &s : msg.sentences())
				{
					json += s;
					json += line_suffix;
				}
				break;
			case MessageFormat::NMEA_TAG:
				msg.getNMEATagBlock(json);
				break;
			case MessageFormat::FULL:
				for (const auto &s : msg.sentences())
				{
					json += s;
					json += " ( ";
					if (msg.getLength() > 0)
					{
						json += "MSG: ";
						json += std::to_string(msg.type());
						json += ", REPEAT: ";
						json += std::to_string(msg.repeat());
						json += ", MMSI: ";
						json += std::to_string(msg.mmsi());
					}
					else
					{
						json += "empty";
					}
					if ((tag.mode & 1) && tag.ppm != PPM_UNDEFINED && tag.level != LEVEL_UNDEFINED)
					{
						char tmp[64];
						std::snprintf(tmp, sizeof(tmp), ", signalpower: %g, ppm: %g", tag.level, tag.ppm);
						json += tmp;
					}
					if (tag.mode & 2)
					{
						json += ", timestamp: ";
						json += msg.getRxTime();
					}
					if (msg.getStation())
					{
						json += ", ID: ";
						json += std::to_string(msg.getStation());
					}
					json += ")\n";
				}
				break;
			case MessageFormat::BINARY_NMEA:
				msg.getBinaryNMEA(json, tag);
				break;
			case MessageFormat::COMMUNITY_HUB:
				if (hub_lines++ % 100 == 0)
					msg.getNMEAJSON(json, tag, include_sample_start, uuid, line_suffix);
				else
					msg.getBinaryNMEA(json, tag);
				break;
			default:
				msg.getNMEAJSON(json, tag, include_sample_start, uuid, line_suffix);
				break;
			}
		}

		// usesJSONStream() drives the stream wiring in Connect(), jsonFormat() the GPS encoding
		bool usesJSONStream() const
		{
			return fmt == MessageFormat::JSON_FULL || fmt == MessageFormat::JSON_ANNOTATED ||
				   fmt == MessageFormat::JSON_SPARSE;
		}

		bool jsonFormat() const
		{
			return usesJSONStream() || fmt == MessageFormat::JSON_NMEA;
		}

		void setUUID(const std::string &arg)
		{
			if (!Util::Helper::isUUID(arg))
				throw std::runtime_error(type + ": invalid UUID: " + arg);
			uuid = arg;
		}

		virtual bool readyToSend() { return true; }
		bool warned_no_formatter = false;
		virtual void sendFormatted(const char *, int, const AIS::Message *, TAG &)
		{
			if (!warned_no_formatter)
			{
				warned_no_formatter = true;
				Error() << type << ": no sendFormatted() and Receive() not overridden — dropping all output.";
			}
		}
		virtual void batchDone(TAG &) {}

	public:
		std::vector<std::string> zones;

		MessageFormat fmt = MessageFormat::JSON_FULL;

		void ConnectMessage(Receiver &r);
		void ConnectJSON(Receiver &r);

		virtual void Start() {}
		virtual void Stop() {}
		bool hasUUID() const { return !uuid.empty(); }
		void Connect(Receiver &r);

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override
		{
			if (!readyToSend())
				return;

			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				formatInto(data[i], tag);
				sendFormatted(json.data(), (int)json.size(), &data[i], tag);
			}
			batchDone(tag);
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag) override
		{
			if (!readyToSend())
				return;

			for (int i = 0; i < len; i++)
			{
				const AIS::Message &msg = *(AIS::Message *)data[i].binary;
				if (!filter.include(msg))
					continue;

				json.clear();
				builder.stringify(data[i], json, line_suffix);
				sendFormatted(json.data(), (int)json.size(), &msg, tag);
			}
			batchDone(tag);
		}

		void Receive(const AIS::GPS *data, int len, TAG &tag) override
		{
			if (!forward_gps || !filter.includeGPS() || !readyToSend())
				return;

			for (int i = 0; i < len; i++)
			{
				json.clear();
				json += jsonFormat() ? data[i].getJSON() : data[i].getNMEA();
				json += line_suffix;
				sendFormatted(json.data(), (int)json.size(), nullptr, tag);
			}
			batchDone(tag);
		}

		void writeJSON(JSON::Writer &w) const
		{
			w.beginObject()
				.kv("type", type)
				.kv("description", description);
			if (!link.empty())
				w.kv("link", link);
			w.key("stats");
			stats.writeJSON(w);
			w.endObject();
		}

		std::string getSourcesStr()
		{
			uint64_t gi = StreamIn<AIS::Message>::getGroupsIn();
			if (gi == GROUPS_ALL)
				return "sources: ALL";
			if (gi == 0)
				return "sources: NONE";
			std::string s;
			for (int i = 0; i < 64; i++)
				if (gi & (1ULL << i))
					s += (s.empty() ? "" : ",") + std::to_string(i + 1);
			return "sources: " + s;
		}

		std::string startInfo()
		{
			std::string s = "msgformat: " + Util::Convert::toString(fmt);
			if (!uuid.empty())
				s += ", uuid: " + uuid;
			const std::string f = filter.Get();
			if (!f.empty())
				s += ", " + f;
			return s + ", " + getSourcesStr();
		}

		OutputMessage() : builder(JSON_DICT_FULL) {}
		OutputMessage(const std::string &d) : Setting(d), builder(JSON_DICT_FULL), type(d) {}

		virtual ~OutputMessage() { Stop(); }

		bool setOptionKey(AIS::Keys key, const std::string &arg)
		{
			switch (key)
			{
			case AIS::KEY_SETTING_JSON_FULL:
				Warning() << "JSON_FULL option is deprecated and will be removed in a future release. Use MSGFORMAT instead.";
				if (Util::Parse::Switch(arg))
					fmt = MessageFormat::JSON_FULL;
				return true;
			case AIS::KEY_SETTING_JSON:
				Warning() << "JSON option is deprecated and will be removed in a future release. Use MSGFORMAT instead.";
				if (Util::Parse::Switch(arg))
					fmt = MessageFormat::JSON_NMEA;
				return true;
			case AIS::KEY_SETTING_MSGFORMAT:
				if (!Util::Parse::OutputFormat(arg, fmt))
					throw std::runtime_error("Unknown message format: " + arg);
				if (fmt == MessageFormat::JSON_ANNOTATED)
					builder.setStringifyEnhanced(true);
				else if (fmt == MessageFormat::JSON_SPARSE)
					builder.setMap(JSON_DICT_SPARSE);
				return true;
			case AIS::KEY_SETTING_DESCRIPTION:
			case AIS::KEY_SETTING_DESC:
				description = arg;
				return true;
			case AIS::KEY_SETTING_LINK:
				link = arg;
				return true;
			case AIS::KEY_SETTING_ZONE:
				Util::Parse::Split(arg, ',', zones);
				return true;
			case AIS::KEY_SETTING_INCLUDE_SAMPLE_START:
				include_sample_start = Util::Parse::Switch(arg);
				return true;
			case AIS::KEY_SETTING_GROUPS_IN:
			{
				uint64_t g = Util::Parse::Integer(arg);
				StreamIn<AIS::Message>::setGroupsIn(g);
				StreamIn<JSON::JSON>::setGroupsIn(g);
				StreamIn<AIS::GPS>::setGroupsIn(g);
				return true;
			}
			default:
				return filter.SetOptionKey(key, arg);
			}
		}

		Setting &SetKey(AIS::Keys key, const std::string &arg) override
		{
			if (!setOptionKey(key, arg))
				throw std::runtime_error(type + " output - unknown option: " + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			return *this;
		}
	};

	// Stub base for output channels whose backing library is not compiled into this build.
	// Configuration is silently accepted (so shared configs don't break across builds);
	// any attempt to actually start the channel fails fast with a build-flag hint.
	class OutputUnavailable : public OutputMessage
	{
		std::string build_flag;

	public:
		OutputUnavailable(const std::string &n, const std::string &f)
			: OutputMessage(n), build_flag(f) {}

		void Start() override
		{
			throw std::runtime_error(type + " support not compiled in. Rebuild with -D" + build_flag + "=ON.");
		}
		Setting &SetKey(AIS::Keys, const std::string &) override { return *this; }
	};
}