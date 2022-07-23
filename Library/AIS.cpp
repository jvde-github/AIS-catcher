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
	Decoder::Decoder()
	{
		DataFCS.resize((MaxBits + 7) / 8, 0);
	}

	char Decoder::NMEAchar(int i)
	{
		return i < 40 ? (char)(i + 48) : (char)(i + 56);
	}

	void Decoder::setBit(int i, bool b)
	{
		if (b)
			DataFCS[i >> 3] |= (1 << (i & 7));
		else
			DataFCS[i >> 3] &= ~(1 << (i & 7));
	}

	bool Decoder::getBit(int i)
	{
		return DataFCS[i >> 3] & (1 << (i & 7));
	}

	int Decoder::NMEAchecksum(const std::string& s)
	{
		int check = 0;
		for (char c : s) check ^= c;
		return check;
	}

	void Decoder::NextState(State s, int pos)
	{
		state = s;
		position = pos;
		one_seq_count = 0;

		switch (s)
		{
		case State::TRAINING: DecoderMessage.Send(DecoderMessages::StartTraining); break;
		case State::STARTFLAG: DecoderMessage.Send(DecoderMessages::StopTraining); break;
		case State::FOUNDMESSAGE: DecoderMessage.Send(DecoderMessages::Reset); break;
		default: break;
		}
	}

	bool Decoder::CRC16(int len)
	{
		const uint16_t checksum = ~0x0F47, poly = 0x8408;
		uint16_t CRC = 0xFFFF;

		for (int i = 0; i < len; i++)
			CRC = ((uint16_t)getBit(i) ^ CRC) & 1 ? (CRC >> 1) ^ poly : CRC >> 1;

		return CRC == checksum;
	}

	char Decoder::getLetter(int pos)
	{
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		// zero padding
		uint8_t b0 = x < nBytes ? DataFCS[x] : 0;
		uint8_t b1 = x + 1 < nBytes ? DataFCS[(int)(x + 1)] : 0;
		uint16_t w = (b0 << 8) | b1;

		const int mask = (1 << 6) - 1;
		return (w >> (16 - 6 - y)) & mask;
	}

	void Decoder::sendNMEA(TAG& tag)
	{
		std::string sentence;
		const std::string comma = ",";
		NMEA nmea;

		int nAISletters = (nBits + 6 - 1) / 6;
		int nSentences = (nAISletters + 56 - 1) / 56;

		for (int s = 0, l = 0; s < nSentences; s++)
		{
			sentence = std::string("AIVDM,") + std::to_string(nSentences) + comma + std::to_string(s + 1) + comma;
			sentence += (nSentences > 1 ? std::to_string(MessageID) : "") + comma + channel + comma;

			for (int i = 0; l < nAISletters && i < 56; i++, l++)
				sentence += NMEAchar(getLetter(l));

			sentence += comma + std::to_string((nSentences > 1 && s == nSentences - 1) ? nAISletters * 6 - nBits : 0);

			char hex[3]; sprintf(hex, "%02X", NMEAchecksum(sentence));
			sentence += std::string("*") + hex;

			nmea.sentence.push_back("!" + sentence);
		}

		nmea.channel = channel;
		nmea.msg = type();
		nmea.repeat = repeat();
		nmea.mmsi = mmsi();
		nmea.data = DataFCS.data();
		nmea.length = nBits;

		if(tag.mode & 2) std::time(&tag.timestamp);
		//if(nmea.msg >= 0 and nmea.msg <= 27)
		Send(&nmea, 1, tag);

		MessageID = (MessageID + 1) % 10;
	}

	bool Decoder::processData(int len, TAG& tag)
	{
		if (len > 16 && CRC16(len))
		{
			nBits = len - 16;
			nBytes = (nBits + 7) / 8;

			// calculate the power of the signal in dB, if requested
			if(tag.mode |= 1) tag.level = 10.0f * log10(tag.level);

			// Populate Byte array and send msg, exclude 16 FCS bits
			sendNMEA(tag);

			return true;
		}
		return false;
	}

	void Decoder::Message(const DecoderMessages& in)
	{
		switch (in)
		{
		case DecoderMessages::Reset: NextState(State::TRAINING, 0); break;
		default: break;
		}
	}

	// returns true if data so far cannot result anymore in a valid message
	bool Decoder::canStop(int len)
	{
		const int END = 24;
		int msg = type();

		switch (len)
		{
		case 8: return msg > 27 || msg == 0;
		case 38: return mmsi() > 999999999;

		case 72 + END: return msg == 10;
		case 144 + END: return msg == 16;
		case 160 + END: return msg == 15 || msg == 20 || msg == 23;
		case 168 + END: return msg == 1 || msg == 2 || msg == 3 || msg == 4 || msg == 7 || msg == 9 || msg == 11  || msg == 18 || msg == 22 || msg == 24 || msg == 25 || msg == 27;
		case 312 + END: return msg == 19;
		case 361 + END: return msg == 21;
		case 424 + END: return msg == 5;
		}
		return false;
	}

	void Decoder::Receive(const FLOAT32* data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			// NRZI
			BIT d = data[i] > 0;
			BIT Bit = !(d ^ prev);
			prev = d;

			// State machine
			// At this stage: "position" bits into sequence, inspect the next bit:
			switch (state)
			{
			case State::TRAINING:
				if (Bit != lastBit) // 01 10
				{
					position++;
				}
				else // 11 or 00
				{
					if (position > MIN_TRAINING_BITS) NextState(State::STARTFLAG, Bit ? 3 : 1); // we are at * in ..0101|01*111110 ..010|*01111110
					else NextState(State::TRAINING, 0);
				}
				break;
			case State::STARTFLAG:

				if (position == 7)
				{
					if (Bit == 0)
					{
						NextState(State::DATAFCS, 0); // 0111111*0....
						level = 0.0f;
					}
					else NextState(State::TRAINING, 0);
				}
				else
				{
					if (Bit == 1) position++;
					else NextState(State::TRAINING, 0);
				}
				break;
			case State::DATAFCS:

				setBit(position++,Bit);

				// add power of signal of bit length
				if(tag.mode |= 1) level += tag.sample_lvl;

				if (Bit == 1)
				{
					if (one_seq_count == 5)
					{
						if(tag.mode |= 1) tag.level = level/position;
						if(processData(position - 7, tag))
							NextState(State::FOUNDMESSAGE, 0);
						NextState(State::TRAINING, 0);
					}
					else
						one_seq_count++;
				}
				else
				{
					if (one_seq_count == 5) position--; // bit-destuff
					one_seq_count = 0;
				}

				if (position == MaxBits || (QuickReset && canStop(position))) NextState(State::TRAINING, 0);
				break;

			default:
				break;
			}
			lastBit = Bit;
		}
	}
}
