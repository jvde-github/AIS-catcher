/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include "Stream.h"
#include "AIS.h"
#include "Property.h"

namespace IO {
	template <typename T>
	class StreamCounter : public StreamIn<T> {
		uint64_t count = 0;
		uint64_t lastcount = 0;
		float rate = 0.0f;

		high_resolution_clock::time_point time_lastupdate;
		float seconds = 0;
		int msg_count = 0;

	public:
		StreamCounter() : StreamIn<T>() {
			resetStatistic();
		}

		void Receive(const T* data, int len, TAG& tag) {
			count += len;
		}

		uint64_t getCount() { return count; }

		void Stamp() {
			auto timeNow = high_resolution_clock::now();
			float seconds = 1e-6f * duration_cast<microseconds>(timeNow - time_lastupdate).count();

			msg_count = count - lastcount;
			rate += 1.0f * (msg_count / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;
		}

		float getRate() { return rate; }
		int getDeltaCount() { return msg_count; }

		void resetStatistic() {
			count = 0;
			time_lastupdate = high_resolution_clock::now();
		}
	};

	template <typename T>
	class SinkFile : public StreamIn<T> {
		std::ofstream file;
		std::string filename;

	public:
		~SinkFile() {
			if (file.is_open())
				file.close();
		}
		void openFile(std::string fn) {
			filename = fn;
			file.open(filename, std::ios::out | std::ios::binary);
		}

		void Receive(const T* data, int len, TAG& tag) {
			file.write((char*)data, len * sizeof(T));
		}
	};

	class SinkScreenString : public StreamIn<std::string> {
	public:
		void Receive(const std::string* data, int len, TAG& tag) {
			for (int i = 0; i < len; i++)
				std::cout << data[i] << std::endl;
		}
	};

	class SinkScreenMessage : public StreamIn<AIS::Message> {
	private:
		OutputLevel level;

	public:
		void setDetail(OutputLevel l) { level = l; }
		void Receive(const AIS::Message* data, int len, TAG& tag);
	};

	class PropertyToJSON : public PropertyStreamIn, public StreamOut<std::string> {
	protected:
		std::string json;
		bool first = true;

		std::string delim() {
			bool f = first;
			first = false;

			if (f) return "";
			return ",";
		}

		std::string jsonify(const std::string& str);

	public:
		virtual void Set(int p, int v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, unsigned v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, float v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, bool v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + (v ? "true" : "false"); }

		virtual void Set(int p, const std::string& v);
		virtual void Set(int p, const std::vector<std::string>& v);
	};
}
