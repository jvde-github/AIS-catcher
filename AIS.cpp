/*
Copyright(c) 2021 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "AIS.h"

// Sources:
//	https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-0-199811-S!!PDF-E.pdf
//	https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
// 	https://gpsd.gitlab.io/gpsd/AIVDM.html

namespace AIS
{
	Decoder::Decoder()
	{
		DataFCS_Bits.resize(MaxBits, 0);
		DataFCS.resize(MaxBits / 8, 0);
	}

	char Decoder::NMEAchar(int i)
	{
		return i < 40 ? (char)(i + 48) : (char)(i + 56);
	}

	int Decoder::NMEAchecksum(std::string s)
	{
		int check = 0;
		for(char c : s) check ^= c;
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

 		for(int i = 0; i < len; i++)
 			CRC = (DataFCS_Bits[i] ^ CRC) & 1 ? (CRC >> 1) ^ poly : CRC >> 1;

		return CRC == checksum;
	}

	void Decoder::setByteData()
	{
		DataFCS.assign(nBytes,0);

		for(int b = 0; b < nBits; b++)
			DataFCS[b >> 3] |= (DataFCS_Bits[b] << (b & 7));
	}

	char Decoder::getFrame(int pos)
	{
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		// zero padding
		uint8_t b0 = x < nBytes ? DataFCS[x] : 0;
		uint8_t b1 = x + 1 < nBytes ? DataFCS[x + 1] : 0;
		uint16_t w = (b0 << 8) | b1;

		const int mask = (1 << 6) - 1;
		return (w >> (16 - 6 - y)) & mask;
	}

	void Decoder::sendNMEA()
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
				sentence += NMEAchar(getFrame(l));

			sentence += comma + std::to_string( (nSentences > 1 && s == nSentences - 1) ? nAISletters * 6 - nBits : 0);

			char hex[3]; sprintf(hex, "%02X", NMEAchecksum(sentence));
			sentence += std::string("*") + hex;

			nmea.sentence.push_back("!" + sentence);
		}

		nmea.channel = channel;
		nmea.msg = DataFCS[0] >> 2;
		nmea.repeat = DataFCS[0] & 3;
		nmea.mmsi = (DataFCS[1]<<22)|(DataFCS[2]<<14)|(DataFCS[3]<<6)|(DataFCS[4]>>2);

		sendOut(&nmea, 1);

		MessageID = (MessageID + 1) % 10;
	}

	bool Decoder::processData(int len)
	{
		if(CRC16(len))
		{
			nBits = len - 16;
			nBytes = (nBits + 7)/8;

			// Populate Byte array and send msg, exclude 16 FCS bits
			setByteData();
			sendNMEA();

			return true;
		}
		return false;
	}

	void Decoder::Message(const DecoderMessages& in)
	{
		switch (in)
		{
		case DecoderMessages::Reset: NextState(State::TRAINING, 0);  break;
		default: break;
		}
	}

	void Decoder::Receive(const FLOAT32* data, int len)
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
					if (position > 10) NextState(State::STARTFLAG, Bit ? 3 : 1); // we are at * in ..0101|01*111110 ..010|*01111110
					else NextState(State::TRAINING, 0);
				}
				break;
			case State::STARTFLAG:

				if (position == 7)
				{
					if (Bit == 0) NextState(State::DATAFCS, 0); // 0111111*0....
					else NextState(State::TRAINING, 0);
				}
				else
				{
					if (Bit == 1) position++;
					else NextState(State::TRAINING, 0);
				}
				break;
			case State::DATAFCS:
				DataFCS_Bits[position++] = Bit;

				if (Bit == 1)
				{
					if (one_seq_count == 5)
					{
						if( processData(position - 7))
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

				if (position == MaxBits) NextState(State::TRAINING, 0);
				break;
			}
			lastBit = Bit;
		}
	}
}
