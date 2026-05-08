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

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "Parse.h"
#include "ADSB.h"
#include "Keys.h"
#include "OutputStats.h"
#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

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
		std::string description, type;
		unsigned long lines_sent = 0;

		// Formats one AIS message into the reusable `json` member buffer.
		// Zero-allocation in steady state: clear() preserves capacity.
		void formatInto(const AIS::Message &msg, TAG &tag,
						bool sample_start, const std::string &uuid, const char *suffix)
		{
			json.clear();
			switch (fmt)
			{
			case MessageFormat::NMEA:
				for (const auto &s : msg.sentences())
				{
					json += s;
					json += suffix;
				}
				break;
			case MessageFormat::NMEA_TAG:
				msg.getNMEATagBlock(json);
				break;
			case MessageFormat::BINARY_NMEA:
				msg.getBinaryNMEA(json, tag);
				break;
			case MessageFormat::COMMUNITY_HUB:
				if (lines_sent > 0 && lines_sent % 100 != 0)
					msg.getBinaryNMEA(json, tag);
				else
					msg.getNMEAJSON(json, tag, sample_start, uuid, suffix);
				break;
			default:
				msg.getNMEAJSON(json, tag, sample_start, uuid, suffix);
				break;
			}
			lines_sent++;
		}

	public:
		std::vector<std::string> zones;

		MessageFormat fmt = MessageFormat::JSON_FULL;

		void ConnectMessage(Receiver &r);
		void ConnectJSON(Receiver &r);

		virtual void Start() {}
		virtual void Stop() {}
		void Connect(Receiver &r);

		void writeJSON(JSON::Writer &w) const
		{
			w.beginObject()
				.kv("type", type)
				.kv("description", description)
				.key("stats");
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
			for (int i = 0; i < 32; i++)
				if (gi & (1ULL << i))
					s += (s.empty() ? "" : ",") + std::to_string(i + 1);
			return "sources: " + s;
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
			case AIS::KEY_SETTING_ZONE:
				Util::Parse::Split(arg, ',', zones);
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