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

#include "AIS.h"

// Sources:
//	https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-0-199811-S!!PDF-E.pdf
//	https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
// 	https://gpsd.gitlab.io/gpsd/AIVDM.html

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

namespace AIS
{

	void Decoder::NextState(State s, int pos)
	{
		state = s;
		position = pos;
		one_seq_count = 0;

		switch (s)
		{
		case State::TRAINING:
			DecoderMessage.Send(DecoderSignals::StartTraining);
			break;
		case State::STARTFLAG:
			DecoderMessage.Send(DecoderSignals::StopTraining);
			break;
		case State::FOUNDMESSAGE:
			DecoderMessage.Send(DecoderSignals::Reset);
			break;
		default:
			break;
		}
	}

	bool Decoder::CRC16(int len)
	{
		const uint16_t checksum = ~0x0F47, poly = 0x8408;
		uint16_t CRC = 0xFFFF;

		for (int i = 0; i < len; i++)
			CRC = ((uint16_t)msg.getBit(i) ^ CRC) & 1 ? (CRC >> 1) ^ poly : CRC >> 1;

		return CRC == checksum;
	}

	bool Decoder::processData(int len, TAG &tag)
	{
		if (len >= 16 && CRC16(len))
		{
			nBits = len - 16;
			nBytes = (nBits + 7) / 8;

			// calculate the power of the signal in dB, if requested and timestamp
			if (tag.mode & 1 && tag.level != 0.0)
				tag.level = 10.0f * log10(tag.level);
			if (tag.mode & 2)
				msg.Stamp();

			// Populate Byte array and send msg, exclude 16 FCS bits
			msg.setChannel(channel);
			msg.setLength(nBits);
			msg.setOwnMMSI(own_mmsi);
			msg.setStartIdx(start_idx);
			msg.setEndIdx(end_idx);

			if (msg.validate())
			{
				msg.buildNMEA(tag);
				Send(&msg, 1, tag);
			}
			else
				Debug() << "AIS: invalid message of type " << msg.type() << " and length " << msg.getLength();

			return true;
		}
		return false;
	}

	void Decoder::Signal(const DecoderSignals &in)
	{
		switch (in)
		{
		case DecoderSignals::Reset:
			NextState(State::TRAINING, 0);
			break;
		default:
			break;
		}
	}

	// returns true if data so far cannot result anymore in a valid message
	bool Decoder::canStop(int len)
	{
		const int END = 24;

		if (len < 6 + END)
			return false;

		int t = msg.type();

		switch (len)
		{
		case 6 + END:
			return t > 28 || t == 0;
		case 8 + 30 + END:
			return msg.mmsi() > 999999999;
		case 72 + END:
			return t == 10;
		case 144 + END:
			return t == 16;
		case 160 + END:
			return t == 15 || t == 20 || t == 23;
		case 168 + END:
			return t == 1 || t == 2 || t == 3 || t == 4 || t == 7 || t == 9 || t == 11 || t == 18 || t == 22 || t == 24 || t == 25 || t == 27 || t == 28;
		case 312 + END:
			return t == 19;
		case 361 + END:
			return t == 21;
		case 424 + END:
			return t == 5;
		}
		return false;
	}

}
