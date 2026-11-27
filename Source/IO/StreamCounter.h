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
#include <ctime>
#include <mutex>

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "ADSB.h"

namespace IO
{
	class StreamCounter : public StreamIn<AIS::Message>, public StreamIn<Plane::ADSB>
	{
		// counted on the decoding thread, read on another: a 64-bit value can tear on 32-bit ARM
		std::mutex mtx;
		uint64_t count = 0;
		uint64_t lastcount = 0;
		// errors: checksum, undersized, oversized
		std::time_t last = 0;
		uint64_t errors[3] = {0, 0, 0};
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
			std::lock_guard<std::mutex> lock(mtx);
			count += len;
			last = data[len - 1].getRxTimeUnix();
			if (tag.quality & MESSAGE_QUALITY_CHECKSUM)
				errors[0] += len;
			if (tag.quality & MESSAGE_QUALITY_UNDERSIZED)
				errors[1] += len;
			if (tag.quality & MESSAGE_QUALITY_OVERSIZED)
				errors[2] += len;
		}

		void Receive(const Plane::ADSB *data, int len, TAG &tag) override
		{
			std::lock_guard<std::mutex> lock(mtx);
			count += len;
			last = data[len - 1].getRxTimeUnix();
		}

		uint64_t getCount()
		{
			std::lock_guard<std::mutex> lock(mtx);
			return count;
		}

		std::time_t getLast()
		{
			std::lock_guard<std::mutex> lock(mtx);
			return last;
		}

		void addErrors(uint64_t out[3])
		{
			std::lock_guard<std::mutex> lock(mtx);
			for (int i = 0; i < 3; i++)
				out[i] += errors[i];
		}

		void Stamp()
		{
			std::lock_guard<std::mutex> lock(mtx);
			auto timeNow = std::chrono::high_resolution_clock::now();
			float seconds = 1e-6f * std::chrono::duration_cast<std::chrono::microseconds>(timeNow - time_lastupdate).count();

			msg_count = (int)(count - lastcount);
			rate += 1.0f * (msg_count / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;
		}

		float getRate() { return rate; }
		int getDeltaCount() { return msg_count; }

		void resetStatistic()
		{
			std::lock_guard<std::mutex> lock(mtx);
			count = 0;
			errors[0] = errors[1] = errors[2] = 0;
			time_lastupdate = std::chrono::high_resolution_clock::now();
		}
	};
}
