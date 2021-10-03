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

#include "Device.h"

namespace Device
{

	class SettingsRAWFile : public DeviceSettings
	{
	private:

		Format format = Format::CU8;
		std::string file;
		bool stereo = true;

	public:

		friend class RAWFile;

		void Print();
		void Set(std::string option, std::string arg);
	};

	class RAWFile : public DeviceBase, public StreamOut<CFLOAT32>
	{
		std::ifstream file;
		std::string filename;
		std::vector<char> buffer;
		std::vector<CFLOAT32> output;
		const int buffer_size = 16 * 16384;
		bool stereo = true;
		Format format = Format::CU8;

	public:

		// Control
		void Play();
		void Stop();

		bool isCallback() { return false; }
		bool isStreaming();

		static void pushDeviceList(std::vector<Description>& DeviceList)
		{
			DeviceList.push_back(Description("FILE", "RAW", "0", 0, Type::RAWFILE));
		}

		std::vector<uint32_t> SupportedSampleRates();

		// Device specific
		void setFormat(Format f) { format = f; }

		void Open(SettingsRAWFile& s);

	};
}
