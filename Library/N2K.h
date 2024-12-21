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

#include <iomanip>

#ifdef HASNMEA2000
#include <N2kMessages.h>
#endif

#include "Message.h"
#include "Stream.h"

namespace AIS {

	class N2KtoMessage : public SimpleStreamInOut<RAW, Message> {
#ifdef HASNMEA2000
		Message msg;

		void onMsg129038(const tN2kMsg& n2, TAG& t);
		void onMsg129793(const tN2kMsg& n2, TAG& t);
		void onMsg129794(const tN2kMsg& n2, TAG& tag);
		void onMsg129798(const tN2kMsg& N2kMsg, TAG& tag);
		void onMsg129802(const tN2kMsg& N2kMsg, TAG& tag);
		void onMsg129039(const tN2kMsg& N2kMsg, TAG& tag);

		public:
			virtual ~N2KtoMessage() {}
			void Receive(const RAW* data, int len, TAG& tag);
#endif
		};
	}
