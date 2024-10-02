/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

namespace AIS {

	enum class State {
		TRAINING,
		STARTFLAG,
		STOPFLAG,
		DATAFCS,
		FOUNDMESSAGE
	};

	class Decoder : public SimpleStreamInOut<FLOAT32, Message>, public SignalIn<DecoderSignals> {
		char channel = '?';
		int station = 0;
		int own_mmsi = -1;

		const int MaxBits = MAX_AIS_LENGTH;
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
		bool processData(int len, TAG& tag);

		bool canStop(int);

		Message msg;

	public:
		virtual ~Decoder() {}

		void setOrigin(char c, int s, int o) {
			channel = c;
			station = s;
			own_mmsi = o;
		}

		void Receive(const FLOAT32* data, int len, TAG& tag);

		// MessageIn
		virtual void Signal(const DecoderSignals& in);
		// MessageOut
		SignalHub<DecoderSignals> DecoderMessage;
	};
}
