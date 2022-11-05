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

#include "Message.h"
#include "Stream.h"
#include "Signals.h"

namespace AIS {

	class NMEA : public SimpleStreamInOut<RAW, Message>, public SignalIn<DecoderSignals> {

		Message msg;
		char channel = '?';

		const std::string header = "!AIVDM";
		std::string sentence;
		std::vector<int> commas;
		int index = 0;

		char NMEAchar(int i) { return i < 40 ? (char)(i + 48) : (char)(i + 56); }

		void parse(TAG& tag);
		void reset();

	public:
		void Receive(const RAW* data, int len, TAG& tag);
	};
}
