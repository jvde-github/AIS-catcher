/*
Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <vector>
#include <chrono>

template<typename T>
class FIFO
{
	std::vector<T> _data;

	int head = 0;
	int tail = 0;

	std::atomic<int> count;

	std::mutex fifo_mutex;
	std::condition_variable fifo_cond;

	int BLOCK_SIZE = 16 * 16384;
	int N_BLOCKS = 2;

	const static int timeout = 2500;

public:

	void Init(int bs = 16 * 16384, int fs = 2)
	{
		BLOCK_SIZE = bs; N_BLOCKS = fs;
		count = head = tail = 0;
		_data.resize((int)(N_BLOCKS * BLOCK_SIZE));
	}

	int BlockSize()
	{
		return BLOCK_SIZE;
	}

	bool Wait()
	{
		if (count == 0)
		{
			std::unique_lock <std::mutex> lock(fifo_mutex);
			fifo_cond.wait_for(lock, std::chrono::milliseconds((int)(timeout)), [this] {return count != 0; });

			if (count == 0) return false;
		}
		return true;
	}

	T* Front()
	{
		return _data.data() + head;
	}
	void Pop()
	{
		if (count > 0)
		{
			head = (head + BLOCK_SIZE) % (int) _data.size();
			count--;
		}
	}
	bool Full()
	{
		return count == N_BLOCKS;
	}

	bool Push(T* data, int sz)
	{
		// size of new tail block including overvlow (i.e. > BLOCK_SIZE)
		int  nbl = tail % BLOCK_SIZE + sz;

		// fits within a block, simplified case
		if (nbl <= BLOCK_SIZE)
		{
			if (count == N_BLOCKS) return false;
			std::memcpy(_data.data() + tail, data, sz);
		}
		else
		{
			// need next block available for overflow
			if (count + 1 >= N_BLOCKS) return false;

			// copy can involve wrap
			int wrap = tail + sz - (int)_data.size();
			if ( wrap < 0 ) wrap = 0;

			std::memcpy(_data.data() + tail, data, sz - wrap);
			std::memcpy(_data.data(), data + sz - wrap, wrap);
		}

		tail = (tail + sz) % (int) _data.size();

		// if we completed a full block, ship it off
		if (nbl >= BLOCK_SIZE)
		{
			{
				std::lock_guard<std::mutex> lock(fifo_mutex);
				count++;
			}

			fifo_cond.notify_one();
		}
		return true;
	}
};
