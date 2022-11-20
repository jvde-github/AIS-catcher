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

#include <iomanip>

#include "Stream.h"
#include "JSON.h"
#include "AIS.h"
#include "Signals.h"
#include "Utilities.h"

namespace AIS {
	class JSONAIS : public SimpleStreamInOut<Message, JSON::JSON> {
		JSON::JSON json;

		void ProcessMsg8Data(const AIS::Message& msg, int len);

		static const std::string sClass;
		static const std::string sDevice;

		const std::string nan = "nan";
		const std::string fastleft = "fastleft";
		const std::string fastright = "fastright";
		const std::string undefined = "Undefined";

		std::string channel, timestamp, datastring, rxtime;
		std::string eta, text, callsign, shipname, destination, name, vendorid;

	protected:
		void U(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void UL(const AIS::Message& msg, int p, int start, int len, float a, float b, unsigned undefined = ~0);
		void S(const AIS::Message& msg, int p, int start, int len, int undefined = ~0);
		void SL(const AIS::Message& msg, int p, int start, int len, float a, float b, int undefined = ~0);
		void E(const AIS::Message& msg, int p, int start, int len, int pmap = 0, const std::vector<std::string>* map = NULL);
		void TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void B(const AIS::Message& msg, int p, int start, int len);
		void X(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0){};

void COUNTRY(const AIS::Message& msg);

		void D(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void T(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void TIMESTAMP(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void ETA(const AIS::Message& msg, int p, int start, int len, std::string& str);

		void Submit(int p, int v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, v);
		}
		void Submit(int p, unsigned v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, v);
		}
		void Submit(int p, float v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, v);
		}
		void Submit(int p, bool v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, v);
		}
		void Submit(int p, const std::string& v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, (std::string*)&v);
		}
		void Submit(int p, const std::vector<std::string>& v) {
			json.object.push_back(JSON::Member());
			json.object.back().Set(p, (std::vector<std::string>*)&v);
		}
	public:
		void Receive(const AIS::Message* data, int len, TAG& tag);
	};
}
