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

#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>
#include <cerrno>
#endif

#include "FileRAW.h"

namespace Device
{

	//---------------------------------------
	// RAW CU8 file

	void RAWFile::ReadAsync()
	{

		try
		{
#ifndef _WIN32
			if (use_raw_stdin)
			{
				while (Device::isStreaming())
				{
					struct pollfd pfd;
					pfd.fd = STDIN_FILENO;
					pfd.events = POLLIN;
					pfd.revents = 0;

					int pr = poll(&pfd, 1, 250);
					if (pr <= 0)
					{
						// timeout or EINTR: re-check isStreaming(); real error: stop
						if (pr < 0 && errno != EINTR)
							break;
						continue;
					}

					ssize_t bytesRead = ::read(STDIN_FILENO, buffer.data(), buffer.size());
					if (bytesRead <= 0)
						break;

					fifo.Push(buffer.data(), (int)bytesRead);
				}
			}
			else
#endif
				while (file && Device::isStreaming())
				{

					if (!file->eof())
					{
						file->read((char *)buffer.data(), buffer.size());
						std::streamsize bytesRead = file->gcount();

						if (bytesRead > 0)
						{
							if (bytesRead < buffer.size())
								std::memset(buffer.data() + bytesRead, 0, buffer.size() - bytesRead);

							fifo.Push(buffer.data(), buffer.size());
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
			StopRequest();
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
			StopRequest();
		}
	}

	bool RAWFile::isReplay()
	{
		bool is_stdin = (filename == "." || filename == "stdin");
#ifndef _WIN32
		struct stat st;
		if (is_stdin)
			return fstat(STDIN_FILENO, &st) == 0 && S_ISREG(st.st_mode);
		return stat(filename.c_str(), &st) != 0 || S_ISREG(st.st_mode);
#else
		return !is_stdin;
#endif
	}

	void RAWFile::Play()
	{
		Device::Play();

		bool is_text = getFormat() == Format::TXT || getFormat() == Format::BASESTATION || getFormat() == Format::BEAST || getFormat() == Format::RAW1090;

		if (!is_text)
		{
			fifo.Init(BUFFER_SIZE_IQ, BUFFER_COUNT);
			buffer.resize(BUFFER_SIZE_IQ);
		}
		else
		{
			fifo.Init(TXT_BLOCK_SIZE, 2 * BUFFER_SIZE_TXT);
			buffer.resize(BUFFER_SIZE_TXT);
		}
		fifo.setWait(lossless);

		if (filename == "." || filename == "stdin")
		{
#ifndef _WIN32
			if (is_text)
			{
				use_raw_stdin = true;
			}
			else
#endif
			{
				file = &std::cin;
			}
		}
		else
		{
			file = new std::ifstream(filename, std::ios::in | std::ios::binary);
		}

		if (!use_raw_stdin && (!file || file->fail()))
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

	Setting &RAWFile::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_FILE:
			filename = arg;
			break;
		case AIS::KEY_SETTING_LOOP:
			loop = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_TXT_BLOCK_SIZE:
			TXT_BLOCK_SIZE = Util::Parse::Integer(arg, 1, 16384);
			break;
		case AIS::KEY_SETTING_LOSSLESS:
			lossless = Util::Parse::Switch(arg);
			break;
		default:
			Device::SetKey(key, arg);
			break;
		}
		return *this;
	}

	std::string RAWFile::Get()
	{
		return Device::Get() + " file " + filename + " loop " + Util::Convert::toString(loop) + " lossless " + Util::Convert::toString(lossless);
	}
}