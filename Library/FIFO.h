/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include <mutex>
#include <condition_variable>

#include <vector>
#include <chrono>
#include <cstring>

// FIFO implementation: input (Push) can be any size, output (Pop) will be of size BLOCK_SIZE


class FIFO {
	std::vector<char> _data;

	int head = 0;
	int tail = 0;

	int blocks_filled = 0;

	std::mutex fifo_mutex;
	std::condition_variable fifo_cond;

	int BLOCK_SIZE = 16 * 16384;
	int N_BLOCKS = 2;

	const static int timeout = 1500;

public:
	void Init(int bs = 16 * 16384, int fs = 2) {
		BLOCK_SIZE = bs;
		N_BLOCKS = fs;
		blocks_filled = head = tail = 0;
		_data.resize((int)(N_BLOCKS * BLOCK_SIZE));
	}

	int BlockSize() {
		return BLOCK_SIZE;
	}

	void Halt() {
		std::lock_guard<std::mutex> lock(fifo_mutex);

		blocks_filled = -1;
		fifo_cond.notify_one();
	}

	bool Wait() {
		std::unique_lock<std::mutex> lock(fifo_mutex);

		if (blocks_filled == 0) {
			fifo_cond.wait_for(lock, std::chrono::milliseconds((int)(timeout)), [this] { return blocks_filled != 0; });
		}
		return blocks_filled > 0;
	}

	char* Front() {
		return _data.data() + head;
	}

	void Pop() {
		std::unique_lock<std::mutex> lock(fifo_mutex);

		if (blocks_filled > 0) {
			head = (head + BLOCK_SIZE) % (int)_data.size();
			blocks_filled--;
		}
	}

	bool Full() {
		std::unique_lock<std::mutex> lock(fifo_mutex);

		return blocks_filled == N_BLOCKS;
	}

	bool Push(char* data, int sz) {
		std::unique_lock<std::mutex> lock(fifo_mutex);

		if (sz <= 0) return true;

		// size of new tail block including overflow (i.e. > BLOCK_SIZE)
		int blocks_ready = (tail % BLOCK_SIZE + sz) / BLOCK_SIZE;
		int blocks_needed = (tail % BLOCK_SIZE + sz - 1) / BLOCK_SIZE + 1;
		int wrap = tail + sz - (int)_data.size();

		if (blocks_filled == -1) return false;
		if (blocks_filled + blocks_needed > N_BLOCKS) return false;

		if (wrap <= 0) {
			std::memcpy(_data.data() + tail, data, sz);
		}
		else {
			std::memcpy(_data.data() + tail, data, sz - wrap);
			std::memcpy(_data.data(), data + sz - wrap, wrap);
		}

		tail = (tail + sz) % (int)_data.size();

		if (blocks_ready > 0) {
			blocks_filled += blocks_ready;
			fifo_cond.notify_one();
		}
		return true;
	}
};
