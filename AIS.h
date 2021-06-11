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

#pragma once

#include "Stream.h"
#include "AIS.h"
#include "Signal.h"

namespace AIS
{
	enum class State { TRAINING, STARTFLAG, STOPFLAG, DATAFCS, FOUNDMESSAGE };

	class Decoder : public SimpleStreamInOut<FLOAT32, NMEA>, public MessageIn<DecoderMessages>
	{
		char channel = '?';

		std::vector<uint8_t> DataFCS;

		const int MaxBits = 512;

		State state = State::TRAINING;

		BIT lastBit = 0;
		BIT prev = 0;

		int MessageID = 0;
		int nBytes = 0;
		int nBits = 0;

		int position = 0;
		int one_seq_count = 0;

		void setBit(int i, bool b);
		bool getBit(int i);

		void NextState(State s, int pos);
		char NMEAchar(int i);
		int NMEAchecksum(std::string);

		void sendNMEA();
		bool CRC16(int len);
		char getLetter(int pos);
		bool processData(int len);

	public:

		Decoder();

		virtual void setChannel(char c) { channel = c; }
		void Receive(const FLOAT32* data, int len);

		// MessageIn
		virtual void Message(const DecoderMessages& in);
		// MessageOut
		MessageHub<DecoderMessages> DecoderMessage;
	};
}
