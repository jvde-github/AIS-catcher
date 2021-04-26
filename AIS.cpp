/*
Copyright(c) 2021 Jasper van den Eshof

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

namespace AIS
{

	Decoder::Decoder()
	{
		channel = 'A';

		DataFCS_Bits.resize(MaxBits, 0);
		DataFCS.resize(MaxBits / 8, 0);
	}

	char Decoder::NMEchar(int i)
	{
		return i < 40 ? (char)(i + 48) : (char)(i + 56);
	}

	void Decoder::NextState(State s, int pos)
	{
		state = s;
		position = pos;
		bit_stuff_count = 0;

		if (s == State::TRAINING)
			DecoderStateMessage.Send(DecoderMessage::StartTraining);
		if (s == State::STARTFLAG)
			DecoderStateMessage.Send(DecoderMessage::StartMessage);
	}

	bool Decoder::CRC16(int len)
	{
		const uint16_t checksum = ~0x0F47;
 		uint16_t CRC = 0xFFFF;

 		for(int i = 0; i < len; i++)
 			CRC = (DataFCS_Bits[i] ^ CRC) & 1 ? (CRC >> 1) ^ 0x8408 : CRC >> 1;

		return CRC == checksum;
	}

	void Decoder::setByteData(int len)
	{
		// reverse bytes (HDLC format)
		for (int i = 0, ptr = 0; i < len/8; i++)
 		{
			DataFCS[i] = 0;
			for (int j = 0; j < 8; j++, ptr++)
				DataFCS[i] |= (DataFCS_Bits[ptr] << j);
		}

	}

	char Decoder::getFrame(int pos, int len)
	{
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		// zero padding
		uint8_t b0 = x < len ? DataFCS[x] : 0;
		uint8_t b1 = x + 1 < len ? DataFCS[x + 1] : 0;
		uint16_t w = (b0 << 8) | b1;

		const int mask = (1 << 6) - 1;
		return (w >> (16 - 6 - y)) & mask;
	}

	void Decoder::SendNMEA(int len)
	{
		std::string sentence = "";
		const std::string comma = ",";
		NMEA nmea;

		int nBytes = len / 8;
		int AISletters = (nBytes * 8 + 5) / 6;
		int nSentences = (len + 6 * 56 - 1) / (6 * 56);
		int frame = 0;

		for (int idx = 0; idx < nSentences; idx++)
		{
			sentence = std::string("AIVDM,") + std::to_string(nSentences) + comma + std::to_string(idx + 1) + comma;

			if (nSentences > 1) sentence += std::to_string(SequenceNumber);
			sentence += comma + channel + comma;

			for (int i = 0; frame < AISletters && i < 56; i++, frame++)
				sentence += NMEchar(getFrame(frame, nBytes));

			if (nSentences > 1 && idx == nSentences - 1)
				sentence += comma + std::to_string(AISletters * 6 - nBytes * 8);
			else
				sentence += std::string(",0");

			int check = 0;
			for(char c : sentence) check ^= c;
			char hex[3]; sprintf(hex, "%02X", check);
			sentence += std::string("*") + hex;

			nmea.sentence.push_back("!" + sentence);
		}

		nmea.channel = channel;
		nmea.msg = getFrame(0,nBytes);
		nmea.mmsi = (DataFCS[1]<<22)|(DataFCS[2]<<14)|(DataFCS[3]<<6)|(DataFCS[4]>>2);

		sendOut(&nmea, 1);

		SequenceNumber = (SequenceNumber + 1) % 10;
	}

	void Decoder::ProcessData(int len)
	{
		int ptr;

		if(CRC16(len))
		{
			// reverse bytes (HDLC format) and exclude 16 FCS bits
			setByteData(len-16);
			SendNMEA(len-16);
		}
	}

	void Decoder::Receive(const BIT* data, int len)
	{

		for (int i = 0; i < len; i++)
		{
			// NRZI
			BIT Bit = !(data[i] ^ prev);
			prev = data[i];

			// State machine
			// At this stage: "position" bits into sequence, inspect the next bit:
			switch (state)
			{
			case State::DATAFCS:
				DataFCS_Bits[position++] = Bit;

				if (Bit == 1)
				{
					if (bit_stuff_count == 5)
					{
						ProcessData(position - 7);
						NextState(State::TRAINING, 0);
					}
					else
						bit_stuff_count++;
				}
				else
				{
					if (bit_stuff_count == 5) position--;
					bit_stuff_count = 0;
				}

				if (position == MaxBits) NextState(State::TRAINING, 0);
				break;

			case State::TRAINING:
				if (Bit != lastBit) // 01 10
				{
					position++;
				}
				else// 11 or 00
				{
					if (position > 10) NextState(State::STARTFLAG, Bit ? 3 : 1); // we are at * in 01*111110 010101010|*01111110
					else NextState(State::TRAINING, 0);
				}
				break;
			case State::STARTFLAG:

				if (position == 7)
				{
					if (Bit == 0) NextState(State::DATAFCS, 0); // 01111110*....
					else NextState(State::TRAINING, 0);
				}
				else
				{
					if (Bit == 1) position++;
					else NextState(State::TRAINING, 0);
				}
				break;
			}
			lastBit = Bit;
		}
	}
}
