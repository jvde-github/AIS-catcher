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

namespace AIS
{
	class N2KtoMessage : public SimpleStreamInOut<RAW, Message>
	{
		Message msg;

#ifdef HASNMEA2000
		struct PosFix
		{
			int lon, lat, accuracy, raim, second;
		};

		// Reads type/repeat/mmsi and writes the common AIS header (bits 0-37) into a cleared msg.
		void startMessage(const tN2kMsg &N2kMsg, int &idx);
		PosFix readPosFix(const tN2kMsg &N2kMsg, int &idx);
		void readCogSog(const tN2kMsg &N2kMsg, int &idx, int &cog, int &sog);
		// Reads the 3-byte radio status block; returns radio, sets channel.
		int readRadioChannel(const tN2kMsg &N2kMsg, int &idx, int &channel);
		void finalize(char channel, TAG &tag);

		void onMsg129038(const tN2kMsg &N2kMsg, TAG &tag); // Type 1, 2, 3  - Position Report
		void onMsg129039(const tN2kMsg &N2kMsg, TAG &tag); // Type 18      - Class B Position Report
		void onMsg129040(const tN2kMsg &N2kMsg, TAG &tag); // Type 19      - Class B Extended Position Report
		void onMsg129041(const tN2kMsg &N2kMsg, TAG &tag); // Type 21      - Aid-to-Navigation Report
		void onMsg129793(const tN2kMsg &N2kMsg, TAG &tag); // Type 4, 11   - UTC/Date & Position Report
		void onMsg129794(const tN2kMsg &N2kMsg, TAG &tag); // Type 5       - Static and Voyage Related Data
		void onMsg129798(const tN2kMsg &N2kMsg, TAG &tag); // Type 9       - Extended Position Report
		void onMsg129802(const tN2kMsg &N2kMsg, TAG &tag); // Type 14      - Safety-Related Broadcast Message
		void onMsg129809(const tN2kMsg &N2kMsg, TAG &tag); // Type 24 A    - Static Data Report, Part A
		void onMsg129810(const tN2kMsg &N2kMsg, TAG &tag); // Type 24 B    - Static Data Report, Part B
#endif

	public:
		virtual ~N2KtoMessage() {}

		void setOwnMMSI(int mmsi) { msg.setOwnMMSI(mmsi); }
#ifdef HASNMEA2000
		void Receive(const RAW *data, int len, TAG &tag);
#endif
	};
}
