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
		JSON::StringBuilder builder;

		OutputStats stats;
		std::string description, type;

	public:
		std::vector<std::string> zones;

		MessageFormat fmt = MessageFormat::JSON_FULL;

		void ConnectMessage(Receiver &r);
		void ConnectJSON(Receiver &r);

	public:
		virtual void Start() {}
		virtual void Stop() {}
		void Connect(Receiver &r);

		std::string getJSON() const
		{
			return "{\"type\":\"" + type + "\"" + ",\"description\":\"" + description + "\"" + ",\"stats\":" + stats.toJSON() + "}";
		}

		std::string getSourcesStr()
		{
			uint64_t gi = StreamIn<AIS::Message>::getGroupsIn();
			if (gi == GROUPS_ALL) return "sources: ALL";
			if (gi == 0) return "sources: NONE";
			std::string s;
			for (int i = 0; i < 32; i++)
				if (gi & (1ULL << i))
					s += (s.empty() ? "" : ",") + std::to_string(i + 1);
			return "sources: " + s;
		}

		OutputMessage() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}
		OutputMessage(const std::string &d) : builder(&AIS::KeyMap, JSON_DICT_FULL), type(d) {}

		virtual ~OutputMessage() { Stop(); }

		bool setOption(std::string option, std::string arg)
		{
			if (option == "JSON_FULL")
			{
				Warning() << "JSON_FULL option is deprecated and will be removed in a future release. Use MSGFORMAT instead.";
				if (Util::Parse::Switch(arg))
					fmt = MessageFormat::JSON_FULL;

				return true;
			}
			else if (option == "JSON")
			{
				Warning() << "JSON option is deprecated and will be removed in a future release. Use MSGFORMAT instead.";
				if (Util::Parse::Switch(arg))
				{
					fmt = MessageFormat::JSON_NMEA;
				}
				return true;
			}
			else if (option == "MSGFORMAT")
			{
				if (!Util::Parse::OutputFormat(arg, fmt))
					throw std::runtime_error("Unknown message format: " + arg);

				if (fmt == MessageFormat::JSON_ANNOTATED)
					builder.setStringifyEnhanced(true);

				return true;
			}
			else if (option == "DESCRIPTION" || option == "DESC")
			{
				description = arg;
				return true;
			}
			else if (option == "ZONE")
			{
				Util::Parse::Split(arg, ',', zones);
				return true;
			}
			else if (option == "GROUPS_IN")
			{
				uint64_t g = Util::Parse::Integer(arg);
				StreamIn<AIS::Message>::setGroupsIn(g);
				StreamIn<JSON::JSON>::setGroupsIn(g);
				StreamIn<AIS::GPS>::setGroupsIn(g);
				return true;
			}
			return filter.SetOption(option, arg);
		}
	};
}