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
	int tail_ptr = 0;

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
		count = 0;
		_data.resize(N_BLOCKS * BLOCK_SIZE);
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
		return _data.data() + head * BLOCK_SIZE;
	}
	void Pop()
	{
		head = (head + 1) % N_BLOCKS;
		count--;
	}
	bool Full()
	{
		// we need one block extra to cater for wraps
		return count >= N_BLOCKS - 1;
	}
	T* Back()
	{
		return _data.data() + tail * BLOCK_SIZE;
	}

	bool Push(T* data, int sz)
	{
		int _ptr = tail * BLOCK_SIZE + tail_ptr;

		if (tail_ptr + sz <= BLOCK_SIZE) // fits within a block
		{
			if (count == N_BLOCKS) return false; // full
			std::memcpy(_data.data() + _ptr, data, sz); // copy within block
		}
		else
		{
			if (count + 2 > N_BLOCKS) return false; // full with 2 blocks needed

			int sz1 = sz, sz2 = 0;

			if (_ptr + sz > (int)_data.size())
			{
				int wrap = _ptr + sz - (int)_data.size();
				sz1 -= wrap;
				sz2 = wrap;
			}

			std::memcpy(_data.data() + _ptr, data, sz1);
			std::memcpy(_data.data(), data + sz1, sz2);

		}
		tail_ptr += sz;

		if (tail_ptr >= BLOCK_SIZE)
		{
			SendBlock();
			tail_ptr -= BLOCK_SIZE;
		}
		return true;
	}

	void SendBlock()
	{
		tail = (tail + 1) % N_BLOCKS;

		{
			std::lock_guard<std::mutex> lock(fifo_mutex);
			count++;
		}

		fifo_cond.notify_one();
	}
};
