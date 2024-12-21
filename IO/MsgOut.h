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
#include <fstream>
#include <iostream>
#include <iomanip>

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "Utilities.h"

#include "Keys.h"
#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

class Receiver;

namespace IO
{

	class OutputJSON : public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public Setting
	{
	public:
		virtual void Start() {}
		virtual void Stop() {}
		void Connect(Receiver &r);

		virtual ~OutputJSON() { Stop(); }
	};

	class OutputMessage : public StreamIn<AIS::Message>, public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public Setting
	{
	protected:
		bool JSON_input = false;

		std::string json;
		AIS::Filter filter;
		JSON::StringBuilder builder;

		void ConnectMessage(Receiver &r);
		void ConnectJSON(Receiver &r);

	public:
		virtual void Start() {}
		virtual void Stop() {}
		void Connect(Receiver &r);

		OutputMessage() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}
		virtual ~OutputMessage() { Stop(); }

		bool setOption(std::string option, std::string arg)
		{
			if (option == "JSON_FULL")
			{
				JSON_input = Util::Parse::Switch(arg);
				return true;
			}
			return filter.SetOption(option, arg);
		}
	};

	template <typename T>
	class StreamCounter : public StreamIn<T>
	{
		uint64_t count = 0;
		uint64_t lastcount = 0;
		float rate = 0.0f;

		high_resolution_clock::time_point time_lastupdate;
		float seconds = 0;
		int msg_count = 0;

	public:
		StreamCounter() : StreamIn<T>()
		{
			resetStatistic();
		}

		virtual ~StreamCounter() {}

		void Receive(const T *data, int len, TAG &tag)
		{
			count += len;
		}

		uint64_t getCount() { return count; }

		void Stamp()
		{
			auto timeNow = high_resolution_clock::now();
			float seconds = 1e-6f * duration_cast<microseconds>(timeNow - time_lastupdate).count();

			msg_count = count - lastcount;
			rate += 1.0f * (msg_count / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;
		}

		float getRate() { return rate; }
		int getDeltaCount() { return msg_count; }

		void resetStatistic()
		{
			count = 0;
			time_lastupdate = high_resolution_clock::now();
		}
	};

	class MessageToFile : public OutputMessage
	{
		std::ofstream file;
		std::string filename;

		bool append_mode = true;

	public:
		~MessageToFile()
		{
			Stop();
		}

		void Start()
		{
			file.open(filename, append_mode ? std::ios::app : std::ios::out);

			if (!file)
			{
				throw std::runtime_error("File: failed to open file - " + filename);
			}
		}

		void Stop()
		{
			if (file.is_open())
				file.close();
		}

		void Receive(const AIS::Message *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
			{
				if (filter.include(data[i]))
				{
					for (const std::string &s : data[0].NMEA)
					{
						file << s << std::endl;
					}
				}
			}
			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
			{
				if (filter.include(*(AIS::Message *)data[i].binary))
				{
					json.clear();
					builder.stringify(data[i], json);
					file << json << std::endl;
				}
			}
			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

		Setting &Set(std::string option, std::string arg)
		{
			Util::Convert::toUpper(option);

			if (option == "GROUPS_IN")
			{
				StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
			}
			else if (option == "FILE")
			{
				filename = arg;
			}
			else if (option == "MODE")
			{
				Util::Convert::toUpper(arg);

				if (arg != "APPEND" && arg != "APP" && arg != "OUT")
					throw std::runtime_error("File output - unknown mode: " + arg);

				append_mode = arg == "APPEND" || arg == "APP";
			}
			else if (!OutputMessage::setOption(option, arg))
			{
				throw std::runtime_error("File output - unknown option: " + option);
			}
			return *this;
		}
	};

	class StringToScreen : public StreamIn<std::string>
	{
	public:
		void Receive(const std::string *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
				std::cout << data[i] << std::endl;
		}
		virtual ~StringToScreen() {}
	};

	class MessageToScreen : public StreamIn<AIS::Message>, public StreamIn<AIS::GPS>, public Setting
	{
	private:
		MessageFormat level;
		AIS::Filter filter;

	public:
		virtual ~MessageToScreen() {}
		void setDetail(MessageFormat l) { level = l; }

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		Setting &Set(std::string option, std::string arg)
		{
			Util::Convert::toUpper(option);

			if (option == "GROUPS_IN")
			{
				StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
			}
			else if (!filter.SetOption(option, arg))
			{
				throw std::runtime_error("Message output - unknown option: " + option);
			}

			return *this;
		}
	};

	class JSONtoScreen : public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public Setting
	{
		JSON::StringBuilder builder;
		std::string json;
		AIS::Filter filter;

	public:
		JSONtoScreen(const std::vector<std::vector<std::string>> *map, int d) : builder(map, d) {}

		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);
		void setMap(int m) { builder.setMap(m); }

		Setting &Set(std::string option, std::string arg)
		{
			Util::Convert::toUpper(option);

			if (option == "GROUPS_IN")
			{
				StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
			}
			else if (!filter.SetOption(option, arg))
			{
				throw std::runtime_error("JSON output - unknown option: " + option);
			}
			return *this;
		}
	};
}
