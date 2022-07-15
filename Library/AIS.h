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

#include "Stream.h"
#include "AIS.h"
#include "Signals.h"

namespace AIS
{
	enum class State { TRAINING, STARTFLAG, STOPFLAG, DATAFCS, FOUNDMESSAGE };

	class Decoder : public SimpleStreamInOut<FLOAT32, NMEA>, public MessageIn<DecoderMessages>
	{
		char channel = '?';

		std::vector<uint8_t> DataFCS;

		const int MaxBits = 512;
		const int MIN_TRAINING_BITS = 4;

		bool QuickReset = true;
		State state = State::TRAINING;

		BIT lastBit = 0;
		BIT prev = 0;

		int MessageID = 0;
		int nBytes = 0;
		int nBits = 0;

		int position = 0;
		int one_seq_count = 0;
		FLOAT32 level = 0.0f;

		void setBit(int i, bool b);
		bool getBit(int i);

		void NextState(State s, int pos);
		static char NMEAchar(int i);
		int NMEAchecksum(const std::string&);

		void sendNMEA(TAG &tag);
		bool CRC16(int len);
		char getLetter(int pos);
		bool processData(int len, TAG &tag);

		bool canStop(int);

		unsigned type() { return  DataFCS[0] >> 2; }
		unsigned repeat() { return  DataFCS[0] & 3;  }
		unsigned mmsi() { return (DataFCS[1] << 22) | (DataFCS[2] << 14) | (DataFCS[3] << 6) | (DataFCS[4] >> 2);  }

	public:

		Decoder();

		virtual void setChannel(char c) { channel = c; }
		void Receive(const FLOAT32* data, int len, TAG& tag);

		// MessageIn
		virtual void Message(const DecoderMessages& in);
		// MessageOut
		MessageHub<DecoderMessages> DecoderMessage;
	};
}
