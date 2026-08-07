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

#include <chrono>

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "ADSB.h"

namespace IO
{
	class StreamCounter : public StreamIn<AIS::Message>, public StreamIn<Plane::ADSB>
	{
		uint64_t count = 0;
		uint64_t lastcount = 0;
		float rate = 0.0f;

		std::chrono::high_resolution_clock::time_point time_lastupdate;
		int msg_count = 0;

	public:
		StreamCounter() : StreamIn<AIS::Message>()
		{
			resetStatistic();
		}

		virtual ~StreamCounter() {}

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<Plane::ADSB>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override
		{
			count += len;
		}

		void Receive(const Plane::ADSB *data, int len, TAG &tag) override
		{
			count += len;
		}

		uint64_t getCount() { return count; }

		void Stamp()
		{
			auto timeNow = std::chrono::high_resolution_clock::now();
			float seconds = 1e-6f * std::chrono::duration_cast<std::chrono::microseconds>(timeNow - time_lastupdate).count();

			msg_count = count - lastcount;
			rate += 1.0f * (msg_count / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;
		}

		float getRate() { return rate; }
		int getDeltaCount() { return msg_count; }

		void resetStatistic()
		{
			count = 0;
			time_lastupdate = std::chrono::high_resolution_clock::now();
		}
	};
}
