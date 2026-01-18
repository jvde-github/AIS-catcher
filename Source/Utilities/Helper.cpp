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
		std::string line, model_name;

		// Raspberry Pi: prefer device-tree compatible (recommended by Raspberry Pi docs)
		// Ref: https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-revision-codes
		{
			auto normalize_rpi_model = [](const std::string &model) -> std::string
			{
				// Keep it intentionally simple: map device-tree models into stable product families.
				if (model == "5-model-b")
					return "Raspberry Pi 5";
				if (model == "4-model-b")
					return "Raspberry Pi 4";
				if (model == "500")
					return "Raspberry Pi 500";
				if (model == "400")
					return "Raspberry Pi 400";
				if (model == "3-model-b" || model == "3-model-b-plus" || model == "3-model-a-plus")
					return "Raspberry Pi 3";
				if (model == "2-model-b")
					return "Raspberry Pi 2";
				// Compute Modules
				if (model == "compute-module")
					return "Raspberry Pi Compute Module 1";
				if (model == "3-compute-module")
					return "Raspberry Pi Compute Module 3";
				if (model == "3-plus-compute-module")
					return "Raspberry Pi Compute Module 3+";
				if (model == "4-compute-module" || model == "4s-compute-module")
					return "Raspberry Pi Compute Module 4";
				if (model == "5-compute-module")
					return "Raspberry Pi Compute Module 5";
				// Future-proofing: some distros/DTs may append qualifiers.
				if (model.find("5-compute-module") == 0)
				{
					if (model.find("lite") != std::string::npos)
						return "Raspberry Pi Compute Module 5 Lite";
					return "Raspberry Pi Compute Module 5";
				}
				if (model == "model-zero" || model == "model-zero-w" || model == "model-zero-2-w")
					return "Raspberry Pi Zero";
				if (model == "model-a" || model == "model-a-plus" || model == "model-b" || model == "model-b-plus" ||
					model == "model-b-rev2")
					return "Raspberry Pi 1";
				return "";
			};

			auto parse_dt_compatible = [&](const std::string &path) -> std::string
			{
				std::ifstream inFile(path, std::ios::in | std::ios::binary);
				if (!inFile.is_open())
					return "";

				std::string token;
				while (std::getline(inFile, token, '\0'))
				{
					// token looks like: "raspberrypi,5-model-b" or "brcm,bcm2712"
					static const std::string prefix = "raspberrypi,";
					if (token.compare(0, prefix.size(), prefix) == 0)
					{
						std::string model = token.substr(prefix.size());
						std::string mapped = normalize_rpi_model(model);
						if (!mapped.empty())
							return mapped;
					}
				}
				return "";
			};

			std::string rpi = parse_dt_compatible("/proc/device-tree/compatible");
			if (rpi.empty())
				rpi = parse_dt_compatible("/sys/firmware/devicetree/base/compatible");
			if (!rpi.empty())
				return rpi;
		}

		// Try device-tree model as a fallback (works for Raspberry Pi and other ARM boards)
		{
			auto read_model = [&](const std::string &path) -> std::string
			{
				std::ifstream inFile(path, std::ios::in | std::ios::binary);
				if (!inFile.is_open() || !std::getline(inFile, line, '\0'))
					return "";
				if (!line.empty() && line.back() == '\0')
					line.pop_back();
				return line;
			};

			std::string model = read_model("/proc/device-tree/model");
			if (model.empty())
				model = read_model("/sys/firmware/devicetree/base/model");
			if (!model.empty())
			{
				// Normalize common Raspberry Pi strings to the simple families requested.
				if (model.find("Raspberry Pi 5") != std::string::npos)
					return "Raspberry Pi 5";
				if (model.find("Raspberry Pi 500") != std::string::npos)
					return "Raspberry Pi 500";
				if (model.find("Raspberry Pi 400") != std::string::npos)
					return "Raspberry Pi 400";
				if (model.find("Raspberry Pi 4") != std::string::npos)
					return "Raspberry Pi 4";
				if (model.find("Raspberry Pi 3") != std::string::npos)
					return "Raspberry Pi 3";
				if (model.find("Raspberry Pi 2") != std::string::npos)
					return "Raspberry Pi 2";
				if (model.find("Compute Module 5") != std::string::npos)
				{
					if (model.find("Lite") != std::string::npos || model.find("lite") != std::string::npos)
						return "Raspberry Pi Compute Module 5 Lite";
					return "Raspberry Pi Compute Module 5";
				}
				if (model.find("Compute Module 4") != std::string::npos)
					return "Raspberry Pi Compute Module 4";
				if (model.find("Compute Module 3+") != std::string::npos)
					return "Raspberry Pi Compute Module 3+";
				if (model.find("Compute Module 3") != std::string::npos)
					return "Raspberry Pi Compute Module 3";
				if (model.find("Compute Module") != std::string::npos)
					return "Raspberry Pi Compute Module 1";
				if (model.find("Raspberry Pi Zero") != std::string::npos)
					return "Raspberry Pi Zero";
				if (model.find("Raspberry Pi Model") != std::string::npos)
					return "Raspberry Pi 1";

				return model;
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

		// Parse cpuinfo for CPU model name (best-effort fallback)
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
							model_name = line.substr(pos + 2);
					}
				}
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
