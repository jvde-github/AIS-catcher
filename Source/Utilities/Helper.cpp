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

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#include "Helper.h"

namespace Util
{
	uint16_t Helper::CRC16(const uint8_t *data, size_t length)
	{
		uint16_t crc = 0xFFFF;
		for (size_t i = 0; i < length; i++)
		{
			crc ^= data[i];
			for (int j = 0; j < 8; j++)
			{
				if (crc & 1)
					crc = (crc >> 1) ^ 0xA001;
				else
					crc >>= 1;
			}
		}
		return crc;
	}

	std::string Helper::readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::binary);

		if (file.fail())
			throw std::runtime_error("cannot read file \"" + filename + "\"");

		file.seekg(0, std::ios::end);
		std::streampos fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		const std::streampos MAX_FILE_SIZE = 10 * 1024 * 1024;
		if (fileSize > MAX_FILE_SIZE)
			throw std::runtime_error("file too large (>10MB): \"" + filename + "\"");

		std::string str((size_t)fileSize, '\0');
		if (fileSize > 0)
			file.read(&str[0], fileSize);
		return str;
	}

	// on macOS fsync does not flush the drive cache, F_FULLFSYNC does
	bool Helper::syncFile(const std::string &path)
	{
#ifdef _WIN32
		// FlushFileBuffers behind _commit rejects read-only handles (issue #617)
		int fd = _open(path.c_str(), _O_RDWR | _O_BINARY);
		if (fd < 0)
			return false;
		bool ok = _commit(fd) == 0;
		_close(fd);
		return ok;
#else
		int fd = ::open(path.c_str(), O_RDONLY);
		if (fd < 0)
			return false;
#if defined(__APPLE__) && defined(F_FULLFSYNC)
		bool ok = ::fcntl(fd, F_FULLFSYNC, 0) != -1 || ::fsync(fd) == 0;
#else
		bool ok = ::fsync(fd) == 0;
#endif
		::close(fd);
		return ok;
#endif
	}

	bool Helper::writeFileAtomic(const std::string &path, const std::string &content, std::string &error)
	{
		const std::string tmp = path + ".tmp";

		std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			error = "cannot open \"" + tmp + "\" for writing";
			return false;
		}

		file.write(content.data(), content.size());
		file.close();

		if (file.fail())
		{
			std::remove(tmp.c_str());
			error = "cannot write \"" + tmp + "\"";
			return false;
		}

		// data must reach the disk before the rename makes it live
		if (!syncFile(tmp))
		{
			std::remove(tmp.c_str());
			error = "cannot sync \"" + tmp + "\" to disk";
			return false;
		}

#ifdef _WIN32
		std::remove(path.c_str());
#endif
		if (std::rename(tmp.c_str(), path.c_str()) != 0)
		{
			std::remove(tmp.c_str());
			error = "cannot rename \"" + tmp + "\" to \"" + path + "\"";
			return false;
		}
		return true;
	}

	int Helper::lsb(uint64_t x)
	{
		for (int i = 0; i < 64; i++)
		{
			if (x & (1ULL << i))
				return i;
		}
		return -1;
	}

	long Helper::getMemoryConsumption()
	{
		long memory = 0;
#ifdef _WIN32
		HANDLE hProcess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
		{
			memory = pmc.WorkingSetSize;
		}
#elif defined(__APPLE__)
		mach_task_basic_info info;
		mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
		{
			memory = info.resident_size;
		}
#else
		std::ifstream statm("/proc/self/statm");
		long size = 0, resident = 0;
		if (statm >> size >> resident)
			memory = resident * sysconf(_SC_PAGESIZE);
#endif
		return memory;
	}

	std::string Helper::getOS()
	{
#ifdef _WIN32
		return "Windows";
#elif __ANDROID__
		return "Android";
#elif __APPLE__
		return "MacOS";
#elif __linux__
		std::string os;

		std::ifstream inFile("/etc/os-release");
		std::string line;
		while (std::getline(inFile, line))
		{
			if (line.substr(0, 11) == "PRETTY_NAME")
			{
				std::size_t start = line.find('"') + 1;
				std::size_t end = line.rfind('"');
				if (start != std::string::npos && end != std::string::npos)
				{
					os = line.substr(start, end - start);
					break;
				}
			}
		}

		if (os.empty())
		{
			os = "Linux";
		}
		return os;
#else
		return "";
#endif
	}

	std::string Helper::getHardware()
	{
#ifdef _WIN32

		return "Windows PC";

#elif __ANDROID__

		return "Android Device";

#elif __APPLE__

		return "Mac";

#elif __linux__
		// Raspberry Pi: prefer device-tree compatible (recommended by Raspberry Pi docs)
		// Ref: https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-revision-codes
		static const struct
		{
			const char *token;
			const char *family;
		} RPI_TOKENS[] = {
			{"5-model-b", "Raspberry Pi 5"},
			{"4-model-b", "Raspberry Pi 4"},
			{"500", "Raspberry Pi 500"},
			{"400", "Raspberry Pi 400"},
			{"3-model-b", "Raspberry Pi 3"},
			{"3-model-b-plus", "Raspberry Pi 3"},
			{"3-model-a-plus", "Raspberry Pi 3"},
			{"2-model-b", "Raspberry Pi 2"},
			{"2-model-b-rev2", "Raspberry Pi 2"},
			{"compute-module", "Raspberry Pi Compute Module 1"},
			{"0-compute-module", "Raspberry Pi Compute Module 0"},
			{"3-compute-module", "Raspberry Pi Compute Module 3"},
			{"3-plus-compute-module", "Raspberry Pi Compute Module 3+"},
			{"4-compute-module", "Raspberry Pi Compute Module 4"},
			{"4s-compute-module", "Raspberry Pi Compute Module 4"},
			{"model-zero", "Raspberry Pi Zero"},
			{"model-zero-w", "Raspberry Pi Zero"},
			{"model-zero-2-w", "Raspberry Pi Zero"},
			{"model-a", "Raspberry Pi 1"},
			{"model-a-plus", "Raspberry Pi 1"},
			{"model-b", "Raspberry Pi 1"},
			{"model-b-plus", "Raspberry Pi 1"},
			{"model-b-rev2", "Raspberry Pi 1"},
		};

		// most specific first: "Raspberry Pi 5" is a substring of "Raspberry Pi 500"
		static const struct
		{
			const char *needle;
			const char *family;
		} RPI_MODELS[] = {
			{"Raspberry Pi 500", "Raspberry Pi 500"},
			{"Raspberry Pi 400", "Raspberry Pi 400"},
			{"Raspberry Pi 5", "Raspberry Pi 5"},
			{"Raspberry Pi 4", "Raspberry Pi 4"},
			{"Raspberry Pi 3", "Raspberry Pi 3"},
			{"Raspberry Pi 2", "Raspberry Pi 2"},
			{"Compute Module 5 Lite", "Raspberry Pi Compute Module 5 Lite"},
			{"Compute Module 5 lite", "Raspberry Pi Compute Module 5 Lite"},
			{"Compute Module 5", "Raspberry Pi Compute Module 5"},
			{"Compute Module 4", "Raspberry Pi Compute Module 4"},
			{"Compute Module 3+", "Raspberry Pi Compute Module 3+"},
			{"Compute Module 3", "Raspberry Pi Compute Module 3"},
			{"Compute Module 0", "Raspberry Pi Compute Module 0"},
			{"Compute Module", "Raspberry Pi Compute Module 1"},
			{"Raspberry Pi Zero", "Raspberry Pi Zero"},
			{"Raspberry Pi Model", "Raspberry Pi 1"},
		};

		for (const char *path : {"/proc/device-tree/compatible", "/sys/firmware/devicetree/base/compatible"})
		{
			std::ifstream inFile(path, std::ios::binary);
			std::string token;
			while (inFile && std::getline(inFile, token, '\0'))
			{
				// token looks like: "raspberrypi,5-model-b" or "brcm,bcm2712"
				if (token.compare(0, 12, "raspberrypi,") != 0)
					continue;
				const std::string model = token.substr(12);
				for (const auto &m : RPI_TOKENS)
					if (model == m.token)
						return m.family;
				// future-proofing: some distros/DTs may append qualifiers
				if (model.compare(0, 16, "5-compute-module") == 0)
					return model.find("lite") != std::string::npos ? "Raspberry Pi Compute Module 5 Lite" : "Raspberry Pi Compute Module 5";
			}
		}

		// device-tree model as a fallback (Raspberry Pi and other ARM boards)
		for (const char *path : {"/proc/device-tree/model", "/sys/firmware/devicetree/base/model"})
		{
			std::ifstream inFile(path, std::ios::binary);
			std::string model;
			if (!inFile || !std::getline(inFile, model, '\0') || model.empty())
				continue;
			for (const auto &m : RPI_MODELS)
				if (model.find(m.needle) != std::string::npos)
					return m.family;
			return model;
		}

		// DMI information (x86/x64 systems and some ARM systems)
		{
			std::string product, vendor;
			std::ifstream dmiProduct("/sys/class/dmi/id/product_name");
			if (std::getline(dmiProduct, product) && !product.empty() &&
				product != "To be filled by O.E.M." && product != "System Product Name")
			{
				std::ifstream dmiVendor("/sys/class/dmi/id/sys_vendor");
				if (std::getline(dmiVendor, vendor) && !vendor.empty() &&
					vendor != "To be filled by O.E.M." && vendor != "System manufacturer")
					return vendor + " " + product;
				return product;
			}
		}

		// cpuinfo model name as a best-effort fallback, last entry wins
		{
			std::string line, model_name;
			std::ifstream inFile("/proc/cpuinfo");
			while (std::getline(inFile, line))
			{
				if (line.compare(0, 10, "model name") == 0)
				{
					std::size_t pos = line.find(": ");
					if (pos != std::string::npos)
						model_name = line.substr(pos + 2);
				}
			}
			if (!model_name.empty())
				return model_name;
		}

		return "Linux System";
#endif

		return "";
	}

	std::vector<std::string> Helper::getFilesWithExtension(const std::string &directory, const std::string &extension)
	{
		std::vector<std::string> files;
		for (const std::string &name : getFilesInDirectory(directory))
		{
			if (name.size() < extension.size())
				continue;
#ifdef _WIN32
			// the old FindFirstFile pattern matched extensions case-insensitively
			if (_stricmp(name.c_str() + name.size() - extension.size(), extension.c_str()) != 0)
				continue;
			files.push_back(directory + "\\" + name);
#else
			if (name.compare(name.size() - extension.size(), extension.size(), extension) != 0)
				continue;
			files.push_back(directory + "/" + name);
#endif
		}
		return files;
	}

	std::vector<std::string> Helper::getFilesInDirectory(const std::string &directory)
	{
		std::vector<std::string> files;

#ifdef _WIN32
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		const std::string search_path = directory + "\\*";
		hFind = FindFirstFileA((LPCSTR)search_path.c_str(), &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			std::string file_name = std::string((char *)ffd.cFileName);
			if (file_name != "." && file_name != "..")
			{
				files.push_back(file_name);
			}
		} while (FindNextFileA(hFind, &ffd) != 0);
		FindClose(hFind);

#else
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(directory.c_str())) != nullptr)
		{
			while ((ent = readdir(dir)) != nullptr)
			{
				std::string file_name = ent->d_name;
				if (file_name != "." && file_name != "..")
				{
					files.push_back(file_name);
				}
			}
			closedir(dir);
		}
#endif
		return files;
	}

	bool Helper::isUUID(const std::string &s)
	{
		if (s.size() != 36)
			return false;
		for (int i = 0; i < 36; i++)
		{
			if (i == 8 || i == 13 || i == 18 || i == 23)
			{
				if (s[i] != '-')
					return false;
			}
			else
			{
				if (!isxdigit(s[i]))
					return false;
			}
		}
		return true;
	}
}
