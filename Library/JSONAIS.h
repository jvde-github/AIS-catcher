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
	class JSONAIS : public StreamIn<Message>, public JSONStreamOut {
		void ProcessMsg8Data(const AIS::Message& msg, int len);

	protected:
		void U(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void UL(const AIS::Message& msg, int p, int start, int len, float a, float b, unsigned undefined = ~0);
		void S(const AIS::Message& msg, int p, int start, int len, int undefined = ~0);
		void SL(const AIS::Message& msg, int p, int start, int len, float a, float b, int undefined = ~0);
		void E(const AIS::Message& msg, int p, int start, int len, int pmap = 0, const std::vector<std::string>* map = NULL);
		void TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void TIMESTAMP(const AIS::Message& msg, int p, int start, int len);
		void ETA(const AIS::Message& msg, int p, int start, int len);
		void B(const AIS::Message& msg, int p, int start, int len);
		void X(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0){};
		void T(const AIS::Message& msg, int p, int start, int len);
		void D(const AIS::Message& msg, int p, int start, int len);
		void MMSI(const AIS::Message& msg);

	public:
		void Receive(const AIS::Message* data, int len, TAG& tag);
	};

}
