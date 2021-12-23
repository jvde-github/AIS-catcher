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

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <vector>
#include <chrono>

class FIFO
{
	std::vector<std::vector<char>> data;
	int head = 0;
	int tail = 0;
	std::atomic<int> count;

	std::mutex fifo_mutex;
	std::condition_variable fifo_cond;

	int BUFFER_SIZE = 16 * 16384;
	int SIZE_FIFO = 2;

public:

	void Init(int bs = 16 * 16384, int fs = 2)
	{
		BUFFER_SIZE = bs; SIZE_FIFO = fs;

		count = 0;

		data.resize(SIZE_FIFO);

		for (int i = 0; i < SIZE_FIFO; i++)
			data[i].resize(BUFFER_SIZE);
	}

	int BufferSize()
	{
		return BUFFER_SIZE;
	}

	bool Wait()
	{
		if (count == 0)
		{
			std::unique_lock <std::mutex> lock(fifo_mutex);
			fifo_cond.wait_for(lock, std::chrono::milliseconds((int)(1000) ), [this] {return count != 0; });

			if (count == 0) return false;
		}
		return true;
	}

	char* Front()
	{
		return data[head].data();
	}
	void Pop()
	{
		head = (head + 1) % SIZE_FIFO;
		count--;
	}
	bool Full()
	{
		return count == SIZE_FIFO;
	}
	char* Back()
	{
		return data[tail].data();
	}
	void Push()
	{
		tail = (tail + 1) % SIZE_FIFO;

		{
			std::lock_guard<std::mutex> lock(fifo_mutex);
			count++;
		}

		fifo_cond.notify_one();
	}
};
