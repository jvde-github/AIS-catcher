/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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
#include "JSON/JSON.h"
#include "Keys.h"
#include "AIS.h"
#include "Signals.h"
#include "Utilities.h"

namespace AIS {
	class JSONAIS : public SimpleStreamInOut<Message, JSON::JSON> {
		JSON::JSON json;

		void ProcessMsg(const AIS::Message& msg, TAG& tag);

		const std::string class_str = "AIS";
		const std::string device = "AIS-catcher";
		const std::string nan = "nan";
		const std::string fastleft = "fastleft";
		const std::string fastright = "fastright";
		const std::string undefined = "Undefined";

		std::string channel, timestamp, datastring, rxtime;
		std::string eta, text, callsign, shipname, destination, name, vendorid;

	protected:
		void ProcessMsg8Data(const AIS::Message& msg);
		void ProcessMsg6Data(const AIS::Message& msg);


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

	public:
		JSONAIS() : json(&AIS::KeyMap) {}

		void Receive(const AIS::Message* data, int len, TAG& tag);
	};
}
