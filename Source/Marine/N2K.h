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

#include <iomanip>

#ifdef HASNMEA2000
#include <N2kMessages.h>
#endif

#include "Message.h"
#include "Stream.h"

namespace AIS {

	class N2KtoMessage : public SimpleStreamInOut<RAW, Message> {
		Message msg;

#ifdef HASNMEA2000

       // Handler for PGN 129038: AIS Type 1, 2, 3 (Position Report).
	   void onMsg129038(const tN2kMsg& n2, TAG& t);

	   // Handler for PGN 129793: AIS Type 4 (and Type 11) (UTC/Date & Position Report).
	   void onMsg129793(const tN2kMsg& n2, TAG& t);

	   // Handler for PGN 129794: AIS Type 5 (Static and Voyage Related Data).
	   void onMsg129794(const tN2kMsg& n2, TAG& tag);

	   // Handler for PGN 129798: AIS Type 9 (Extended Position Report).
	   void onMsg129798(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129802: AIS Type 14 (Safety-Related Broadcast Message).
	   void onMsg129802(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129039: AIS Type 18 (Class B Position Report).
	   void onMsg129039(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129040: AIS Type 19 (Class B Extended Position Report).
	   void onMsg129040(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129041: AIS Type 21 (Aid-to-Navigation Report).
	   void onMsg129041(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129809: AIS Type 24, Part A (Static Data Report - Part A).
	   void onMsg129809(const tN2kMsg& N2kMsg, TAG& tag);

	   // Handler for PGN 129810: AIS Type 24, Part B (Static Data Report - Part B).
	   void onMsg129810(const tN2kMsg& N2kMsg, TAG& tag);

		public:
			virtual ~N2KtoMessage() {}
			void Receive(const RAW* data, int len, TAG& tag);
#endif
		public:
			void setOwnMMSI(int mmsi) { msg.setOwnMMSI(mmsi); }

};
	}
