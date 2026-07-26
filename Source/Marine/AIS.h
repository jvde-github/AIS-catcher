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

#include "Message.h"
#include "Stream.h"
#include "Signals.h"

namespace AIS
{

	enum class State
	{
		TRAINING,
		STARTFLAG,
		STOPFLAG,
		DATAFCS,
		FOUNDMESSAGE
	};

	class Decoder : public SimpleStreamInOut<FLOAT32, Message>, public SignalIn<DecoderSignals>
	{
		char channel = '?';
		int station = 0;
		int own_mmsi = -1;

		const int MaxBits = MAX_AIS_FRAME_LENGTH;
		const int MIN_TRAINING_BITS = 4;

		bool QuickReset = true;
		State state = State::TRAINING;

		BIT lastBit = 0;
		BIT prev = 0;

		int nBytes = 0;
		int nBits = 0;

		int position = 0;
		int one_seq_count = 0;
		FLOAT32 level = 0.0f;

		void NextState(State s, int pos);

		bool CRC16(int len);
		bool processData(int len, TAG &tag);

		bool canStop(int);

		Message msg;

		long long start_idx = 0;
		long long end_idx = 0;

	public:
		virtual ~Decoder() {}

		void setOrigin(char c, int s, int o)
		{
			channel = c;
			station = s;
			own_mmsi = o;
		}

		void Receive(const FLOAT32 *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
				Run(data[i], tag);
		}

		// Per-bit kernel: Receive() loops over this; the V2 engine calls it
		// directly. Returns FOUNDMESSAGE on the completing bit, else the
		// resting state.
		State Run(FLOAT32 sample, TAG &tag)
		{
			// NRZI
			BIT d = sample > 0;
			BIT Bit = !(d ^ prev);
			prev = d;

			bool found = false;

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
					if (position > MIN_TRAINING_BITS)
					{
						start_idx = tag.sample_idx;
						NextState(State::STARTFLAG, Bit ? 3 : 1); // we are at * in ..0101|01*111110 ..010|*01111110
					}
					else
						NextState(State::TRAINING, 0);
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
					else
						NextState(State::TRAINING, 0);
				}
				else
				{
					if (Bit == 1)
						position++;
					else
						NextState(State::TRAINING, 0);
				}
				break;
			case State::DATAFCS:

				msg.setBit(position++, Bit);

				// add power of signal of bit length
				if (tag.mode & 1)
					level += tag.sample_lvl;

				if (Bit == 1)
				{
					if (one_seq_count == 5)
					{
						if (tag.mode & 1)
							tag.level = level / position;
						end_idx = tag.sample_idx;
						found = processData(position - 7, tag);
						if (found)
							NextState(State::FOUNDMESSAGE, 0);
						NextState(State::TRAINING, 0);
					}
					else
						one_seq_count++;
				}
				else
				{
					if (one_seq_count == 5)
						position--; // bit-destuff
					one_seq_count = 0;
				}

				if (position == MaxBits || (QuickReset && canStop(position)))
					NextState(State::TRAINING, 0);
				break;

			default:
				break;
			}
			lastBit = Bit;
			return found ? State::FOUNDMESSAGE : state;
		}

		void reset() { NextState(State::TRAINING, 0); }
		State getState() const { return state; }
		long long getStartIdx() const { return start_idx; }

		// MessageIn
		virtual void Signal(const DecoderSignals &in);
		// MessageOut
		SignalHub<DecoderSignals> DecoderMessage;
	};
}
