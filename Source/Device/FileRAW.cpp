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

#include <cstring>

#include "FileRAW.h"

namespace Device
{

	//---------------------------------------
	// RAW CU8 file

	void RAWFile::ReadAsync()
	{

		try
		{
			while (file && Device::isStreaming())
			{

				if (!file->eof())
				{
					file->read((char *)buffer.data(), buffer.size());
					std::streamsize bytesRead = file->gcount();

					if (bytesRead > 0) {
						if(bytesRead < buffer.size())
							std::memset(buffer.data() + bytesRead, 0, buffer.size() - bytesRead);
	
						fifo.Push(buffer.data(), buffer.size(), true);
					}
				}
				else
				{
					if (loop)
					{
						file->clear();
						file->seekg(0, std::ios::beg);
					}
					else
					{
						break;
					}
				}
			}
		}
		catch (std::exception &e)
		{
			Error() << "RAWFile ReadAsync: " << e.what();
			std::terminate();
		}

		fifo.PushFinished();
		eoi = true;
	}

	void RAWFile::Run()
	{
		RAW r = {getFormat(), fifo.Front(), fifo.BlockSize()};

		try
		{
			while (isStreaming())
			{
				if (fifo.Wait())
				{
					int nblocks = -1;
					r.data = fifo.Front(nblocks);
					r.size = nblocks * fifo.BlockSize();

					Send(&r, 1, tag);
					fifo.Pop(nblocks);
				}
				else
				{
					if (eoi && isStreaming())
						done = true;
					else if (isStreaming() && getFormat() != Format::TXT)
						Error() << "FILE: timeout.";
				}
			}
		}
		catch (std::exception &e)
		{
			Error() << "RAWFile Run: " << e.what();
			std::terminate();
		}
	}

	void RAWFile::Play()
	{
		Device::Play();

		bool is_stdin = (filename == "." || filename == "stdin");

		if (getFormat() != Format::TXT && getFormat() != Format::BASESTATION && getFormat() != Format::BEAST && getFormat() != Format::RAW1090)
		{
			fifo.Init(BUFFER_SIZE, BUFFER_COUNT);
			buffer.resize(BUFFER_SIZE);
		}
		else
		{
			fifo.Init(TXT_BLOCK_SIZE, BUFFER_SIZE);

			if(is_stdin)
				buffer.resize(TXT_BLOCK_SIZE);
			else
				buffer.resize(BUFFER_SIZE);
		}

		if (is_stdin)
		{
			file = &std::cin;
		}
		else
		{
			file = new std::ifstream(filename, std::ios::in | std::ios::binary);
		}

		if (!file || file->fail())
			throw std::runtime_error("FILE: Cannot open input.");

		done = false;

		read_thread = std::thread(&RAWFile::ReadAsync, this);
		run_thread = std::thread(&RAWFile::Run, this);
	}

	void RAWFile::Stop()
	{
		if (Device::isStreaming())
		{
			Device::Stop();
			fifo.Halt();

			if (read_thread.joinable())
				read_thread.join();
			if (run_thread.joinable())
				run_thread.join();
		}
	}

	void RAWFile::Close()
	{
		if (file && file != &std::cin)
		{
			delete file;
			file = nullptr;
		}
	}

	Setting &RAWFile::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			filename = arg;
		}
		else if (option == "LOOP")
		{
			loop = Util::Parse::Switch(arg);
		}
		else if (option == "TXT_BLOCK_SIZE")
		{
			TXT_BLOCK_SIZE = Util::Parse::Integer(arg, 1, 16384);
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string RAWFile::Get()
	{
		return Device::Get() + " file " + filename + " loop " + Util::Convert::toString(loop);
	}
}