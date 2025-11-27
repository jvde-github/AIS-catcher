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

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
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
		std::ifstream file(filename);

		if (file.fail())
			throw std::runtime_error("cannot read file \"" + filename + "\"");

		// Get file size first
		file.seekg(0, std::ios::end);
		std::streampos fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		// Limit file size to 10MB to prevent memory exhaustion
		const std::streampos MAX_FILE_SIZE = 10 * 1024 * 1024;
		if (fileSize > MAX_FILE_SIZE)
			throw std::runtime_error("file too large (>10MB): \"" + filename + "\"");

		std::string str, line;
		while (std::getline(file, line))
			str += line + '\n';
		return str;
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
		int memory = 0;
#ifdef _WIN32
		HANDLE hProcess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
		{
			memory = pmc.WorkingSetSize;
		}
#else
		std::ifstream statm("/proc/self/statm");
		if (statm.is_open())
		{
			std::string line;
			std::getline(statm, line);
			std::stringstream ss(line);
			long size, resident, shared, text, lib, data, dt;
			ss >> size >> resident >> shared >> text >> lib >> data >> dt;
			memory = resident * sysconf(_SC_PAGESIZE);
		}
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
		std::string line, model_name, revision;

		// Try device-tree first (works for Raspberry Pi and other ARM boards)
		{
			std::ifstream inFile("/proc/device-tree/model");
			if (inFile.is_open() && std::getline(inFile, line))
			{
				// Remove null terminator if present
				if (!line.empty() && line[line.length() - 1] == '\0')
					line.resize(line.length() - 1);
				return line;
			}
		}

		// Try DMI information (works for x86/x64 systems and some ARM systems)
		{
			std::ifstream dmiProduct("/sys/class/dmi/id/product_name");
			if (dmiProduct.is_open())
			{
				std::string product;
				if (std::getline(dmiProduct, product) && !product.empty() &&
					product != "To be filled by O.E.M." &&
					product != "System Product Name")
				{
					std::ifstream dmiVendor("/sys/class/dmi/id/sys_vendor");
					if (dmiVendor.is_open())
					{
						std::string vendor;
						if (std::getline(dmiVendor, vendor) && !vendor.empty() &&
							vendor != "To be filled by O.E.M." &&
							vendor != "System manufacturer")
						{
							return vendor + " " + product;
						}
					}
					return product;
				}
			}
		}

		// Parse cpuinfo for Raspberry Pi revision codes and CPU model
		{
			std::ifstream inFile("/proc/cpuinfo");
			if (inFile.is_open())
			{
				while (std::getline(inFile, line))
				{
					if (line.substr(0, 10) == "model name")
					{
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos)
						{
							model_name = line.substr(pos + 2);
						}
					}
					else if (line.substr(0, 8) == "Revision")
					{
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos)
						{
							revision = line.substr(pos + 2);
						}
					}
				}
			}
		}

		// Raspberry Pi revision lookup table
		if (!revision.empty())
		{
			static const std::unordered_map<std::string, std::string> rpi_revisions = {
				{"900021", "Raspberry Pi A+ 1.1"},
				{"900032", "Raspberry Pi B+ 1.2"},
				{"900092", "Raspberry Pi Zero 1.2"},
				{"900093", "Raspberry Pi Zero 1.3"},
				{"9000c1", "Raspberry Pi Zero W 1.1"},
				{"9020e0", "Raspberry Pi 3A+ 1.0"},
				{"920092", "Raspberry Pi Zero 1.2"},
				{"920093", "Raspberry Pi Zero 1.3"},
				{"900061", "Raspberry Pi CM1 1.1"},
				{"a01040", "Raspberry Pi 2B 1.0"},
				{"a01041", "Raspberry Pi 2B 1.1"},
				{"a02082", "Raspberry Pi 3B 1.2"},
				{"a020a0", "Raspberry Pi CM3 1.0"},
				{"a020d3", "Raspberry Pi 3B+ 1.3"},
				{"a02042", "Raspberry Pi 2B (with BCM2837) 1.2"},
				{"a21041", "Raspberry Pi 2B 1.1"},
				{"a22042", "Raspberry Pi 2B (with BCM2837) 1.2"},
				{"a22082", "Raspberry Pi 3B 1.2"},
				{"a220a0", "Raspberry Pi CM3 1.0"},
				{"a32082", "Raspberry Pi 3B 1.2"},
				{"a52082", "Raspberry Pi 3B 1.2"},
				{"a22083", "Raspberry Pi 3B 1.3"},
				{"a02100", "Raspberry Pi CM3+ 1.0"},
				{"a03111", "Raspberry Pi 4B 1.1"},
				{"b03111", "Raspberry Pi 4B 1.1"},
				{"b03112", "Raspberry Pi 4B 1.2"},
				{"b03114", "Raspberry Pi 4B 1.4"},
				{"b03115", "Raspberry Pi 4B 1.5"},
				{"c03111", "Raspberry Pi 4B 1.1"},
				{"c03112", "Raspberry Pi 4B 1.2"},
				{"c03114", "Raspberry Pi 4B 1.4"},
				{"c03115", "Raspberry Pi 4B 1.5"},
				{"d03114", "Raspberry Pi 4B 1.4"},
				{"d03115", "Raspberry Pi 4B 1.5"},
				{"c03130", "Raspberry Pi 400 1.0"},
				{"a03140", "Raspberry Pi CM4 1.0"},
				{"b03140", "Raspberry Pi CM4 1.0"},
				{"c03140", "Raspberry Pi CM4 1.0"},
				{"d03140", "Raspberry Pi CM4 1.0"},
				{"902120", "Raspberry Pi Zero 2 W 1.0"},
				{"c04170", "Raspberry Pi 5 8GB"},
				{"d04170", "Raspberry Pi 5 4GB"},
				{"c04171", "Raspberry Pi 5 8GB"},
				{"d04171", "Raspberry Pi 5 4GB"},
				{"902121", "Raspberry Pi Zero 2 W 1.0"}};

			std::unordered_map<std::string, std::string>::const_iterator it = rpi_revisions.find(revision);
			if (it != rpi_revisions.end())
			{
				return it->second;
			}
		}

		// Return CPU model name if available
		if (!model_name.empty())
			return model_name;

		return "Linux System";
#endif

		return "";
	}

	std::vector<std::string> Helper::getFilesWithExtension(const std::string &directory, const std::string &extension)
	{
		std::vector<std::string> files;

#ifdef _WIN32
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		const std::string search_path = directory + "\\*" + extension;
		hFind = FindFirstFileA((LPCSTR)search_path.c_str(), &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			std::string full_path = directory + "\\" + std::string((char *)ffd.cFileName);
			files.push_back(full_path);
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
				if (file_name.length() >= extension.length() &&
					file_name.compare(file_name.length() - extension.length(), extension.length(), extension) == 0)
				{
					std::string full_path = directory + "/" + file_name;
					files.push_back(full_path);
				}
			}
			closedir(dir);
		}
#endif
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
