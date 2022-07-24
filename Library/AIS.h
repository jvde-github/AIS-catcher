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

#define MAX_AIS_LENGTH  (128 * 8)

    class Message
    {
    public:

        std::vector<std::string> sentence;
        char channel;
        uint8_t data[128];
        int length;

        unsigned type() const
        {
            return data[0] >> 2;
        }

        unsigned repeat() const
        {
            return data[0] & 3;
        }

        unsigned mmsi() const
        {
            return (data[1] << 22) | (data[2] << 14) | (data[3] << 6) | (data[4] >> 2);
        }

        unsigned getUint(int start, int len) const
        {
            // max unsigned integers are 30 bits in AIS standard
            const uint8_t ones = 0xFF;
            const uint8_t start_mask[] = { ones, ones >> 1, ones >> 2, ones >> 3, ones >> 4, ones >> 5, ones >> 6, ones >> 7 };

            // we start 2nd part of first byte and stop first part of last byte
            int i = start >> 3;
            unsigned u = data[i] & start_mask[start & 7];
            int remaining = len - 8 + (start & 7);

            // first byte is last byte
            if (remaining <= 0)
            {
                return u >> (-remaining);
            }
            // add full bytes
            while (remaining >= 8)
            {
                u <<= 8;
                u |= data[++i];
                remaining -= 8;
            }
            // make room for last bits if needed
            if (remaining > 0)
            {
                u <<= remaining;
                u |= data[++i] >> (8 - remaining);
            }

            return u;
        }

        int getInt(int start, int len) const
        {
            const unsigned ones = ~0;
            unsigned u = getUint(start, len);

            // extend sign bit for the full bit
            if (u & (1 << (len - 1))) u |= ones << len;
            return (int)u;
        }

        std::string getText(int start, int len) const
        {

            int end = start + len;
            std::string text = "";
            text.reserve((len + 5) / 6);

            while (start < end)
            {
                int c = getUint(start, 6);

                // 0       ->   @ and ends the string
                // 1 - 31  ->   65+ ( i.e. setting bit 6 )
                // 32 - 63 ->   32+ ( i.e. doing nothing )

                if (!c) break;
                if (!(c & 32)) c |= 64;
                text += (char)c;
                start += 6;
            }
            return text;
        }

        void setBit(int i, bool b)
        {
            if (b)
                data[i >> 3] |= (1 << (i & 7));
            else
                data[i >> 3] &= ~(1 << (i & 7));
        }

        bool getBit(int i) const
        {
            return data[i >> 3] & (1 << (i & 7));
        }

        char getLetter(int pos, int nBytes) const
        {
            int x = (pos * 6) >> 3, y = (pos * 6) & 7;

            // zero padding
            uint8_t b0 = x < nBytes ? data[x] : 0;
            uint8_t b1 = x + 1 < nBytes ? data[(int)(x + 1)] : 0;
            uint16_t w = (b0 << 8) | b1;

            const int mask = (1 << 6) - 1;
            return (w >> (16 - 6 - y)) & mask;
        }
    };

	enum class State { TRAINING, STARTFLAG, STOPFLAG, DATAFCS, FOUNDMESSAGE };

	class Decoder : public SimpleStreamInOut<FLOAT32, Message>, public SignalIn<DecoderSignals>
	{
		char channel = '?';

		const int MaxBits = MAX_AIS_LENGTH;
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

		void NextState(State s, int pos);
		static char NMEAchar(int i);
		int NMEAchecksum(const std::string&);

		void sendNMEA(TAG &tag);
		bool CRC16(int len);
		bool processData(int len, TAG &tag);

		bool canStop(int);

		Message msg;
	public:

		Decoder();

		virtual void setChannel(char c) { channel = c; }
		void Receive(const FLOAT32* data, int len, TAG& tag);

		// MessageIn
		virtual void Signal(const DecoderSignals& in);
		// MessageOut
		SignalHub<DecoderSignals> DecoderMessage;
	};
}
