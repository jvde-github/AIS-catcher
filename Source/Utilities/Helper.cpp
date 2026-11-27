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

#include <climits>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <random>
#include <stdexcept>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <pthread.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "Helper.h"
#include <cstdlib>
#include <cmath>
#include "Convert.h"
#include "Common.h"

bool isFinite(double v) { return std::isfinite(v); }

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

	std::string Helper::randomHex(size_t length)
	{
		static const char *digits = "0123456789abcdef";

		std::random_device rd;
		std::string s;
		while (s.size() < length)
		{
			uint32_t r = rd();
			for (int j = 28; j >= 0 && s.size() < length; j -= 4)
				s += digits[(r >> j) & 0xF];
		}
		return s;
	}

	std::string Helper::readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::binary);

		if (file.fail())
			throw std::runtime_error("cannot read file \"" + filename + "\"");

		file.seekg(0, std::ios::end);
		std::streampos fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		if (fileSize < 0)
			throw std::runtime_error("cannot read file \"" + filename + "\"");

		const std::streampos MAX_FILE_SIZE = 10 * 1024 * 1024;
		if (fileSize > MAX_FILE_SIZE)
			throw std::runtime_error("file too large (>10MB): \"" + filename + "\"");

		std::string str((size_t)fileSize, '\0');
		if (fileSize > 0 && !file.read(&str[0], fileSize))
			throw std::runtime_error("cannot read file \"" + filename + "\"");
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

	bool Helper::createTemp(const std::string &tmp, const std::string &target, bool owner_only)
	{
#ifdef _WIN32
		(void)owner_only;
		return true;
#else
		struct stat st;
		const bool exists = ::stat(target.c_str(), &st) == 0;
		const mode_t mode = exists ? (st.st_mode & 07777) : owner_only ? 0600 : 0666;
		::unlink(tmp.c_str());
		const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
		if (fd < 0)
			return false;
		// the target's own mode and 0600 past the umask; any other new file as the umask leaves it
		const bool ok = !(exists || owner_only) || ::fchmod(fd, mode) == 0;
		::close(fd);
		return ok;
#endif
	}

	bool Helper::replaceFile(const std::string &from, const std::string &to)
	{
#ifdef _WIN32
		return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		return std::rename(from.c_str(), to.c_str()) == 0;
#endif
	}

	bool Helper::writeFileAtomic(const std::string &path, const std::string &content, std::string &error, bool owner_only)
	{
		const std::string tmp = path + ".tmp";

		if (!createTemp(tmp, path, owner_only))
		{
			error = "cannot create \"" + tmp + "\"";
			return false;
		}

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

		if (!replaceFile(tmp, path))
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

	static uint64_t memoryInUse()
	{
		uint64_t memory = 0;
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
			memory = (uint64_t)resident * sysconf(_SC_PAGESIZE);
#endif
		return memory;
	}

	static double cpuSeconds()
	{
#ifdef _WIN32
		FILETIME created, ended, kernel, user;
		if (!GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user))
			return 0;
		const auto ticks = [](const FILETIME &t) { return ((unsigned long long)t.dwHighDateTime << 32) | t.dwLowDateTime; };
		return (ticks(kernel) + ticks(user)) / 1e7;
#else
		struct rusage r;
		if (getrusage(RUSAGE_SELF, &r) != 0)
			return 0;
		return r.ru_utime.tv_sec + r.ru_stime.tv_sec + (r.ru_utime.tv_usec + r.ru_stime.tv_usec) / 1e6;
#endif
	}

	static const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
	static const double cpu_started = cpuSeconds();

	static std::string osName()
	{
#ifdef _WIN32
		return "Windows";
#elif __ANDROID__
		return "Android";
#elif __APPLE__
		return "MacOS";
#elif __linux__
		return "Linux";
#else
		return "";
#endif
	}

	static std::string detectProcessor()
	{
		std::string s;
#ifdef _WIN32
#if defined(_M_ARM64)
		s = "arm64";
#elif defined(_M_X64)
		s = "x86_64";
#else
		s = "x86";
#endif
#else
		struct utsname u;
		if (uname(&u) == 0)
			s = u.machine;
#endif
		return s + (s.empty() ? "" : ", ") + std::to_string(sizeof(void *) * 8) + "-bit build";
	}

	static std::string detectHardware()
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

		// most specific first: "raspberry pi 5" is a substring of "raspberry pi 500"
		static const struct
		{
			const char *needle;
			const char *family;
		} RPI_MODELS[] = {
			{"raspberry pi 500", "Raspberry Pi 500"},
			{"raspberry pi 400", "Raspberry Pi 400"},
			{"raspberry pi 5", "Raspberry Pi 5"},
			{"raspberry pi 4", "Raspberry Pi 4"},
			{"raspberry pi 3", "Raspberry Pi 3"},
			{"raspberry pi 2", "Raspberry Pi 2"},
			{"compute module 5 lite", "Raspberry Pi Compute Module 5 Lite"},
			{"compute module 5", "Raspberry Pi Compute Module 5"},
			{"compute module 4", "Raspberry Pi Compute Module 4"},
			{"compute module 3+", "Raspberry Pi Compute Module 3+"},
			{"compute module 3", "Raspberry Pi Compute Module 3"},
			{"compute module 0", "Raspberry Pi Compute Module 0"},
			{"compute module", "Raspberry Pi Compute Module 1"},
			{"raspberry pi zero", "Raspberry Pi Zero"},
			{"raspberry pi model", "Raspberry Pi 1"},
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
			std::string model_lc = model;
			Util::Convert::toLower(model_lc);
			for (const auto &m : RPI_MODELS)
				if (model_lc.find(m.needle) != std::string::npos)
					return m.family;
			return model;
		}

		return "Linux System";
#endif

		return "";
	}

#if defined(__linux__) && !defined(__ANDROID__)
	static bool readWifi(std::istream &route, std::istream &wireless, Helper::SystemInfo &info)
	{
		std::string line, name;
		unsigned long long best = ULLONG_MAX;
		while (std::getline(route, line))
		{
			std::istringstream f(line);
			std::string iface, dest, gateway, mask;
			unsigned flags = 0;
			long refcnt = 0, use = 0;
			unsigned long long metric = 0;
			if (f >> iface >> dest >> gateway >> std::hex >> flags >> std::dec >> refcnt >> use >> metric >> mask && dest == "00000000" && mask == "00000000" && (flags & 1) && metric < best)
			{
				best = metric;
				name = iface;
			}
		}
		while (!name.empty() && std::getline(wireless, line))
		{
			std::istringstream f(line);
			std::string iface, status;
			float link = 0, level = 0;
			if (f >> iface >> status >> link >> level && iface == name + ":")
			{
				const int dbm = (int)level - (level > 127 ? 256 : 0);
				if (dbm >= 0 || dbm < -150)
					return false;
				info.wifi_dbm = dbm;
				info.wifi_quality = dbm >= -40 ? 100 : dbm <= -110 ? 0 : (dbm + 110) * 100 / 70;
				return true;
			}
		}
		return false;
	}
#endif

	Helper::SystemInfo Helper::getSystemInfo()
	{
		static std::mutex mtx;
		static SystemInfo info;
		static bool read = false, measured = false;
		static std::chrono::steady_clock::time_point read_at = started;
		static double cpu_at = cpu_started;

		std::lock_guard<std::mutex> lock(mtx);
		const auto now = std::chrono::steady_clock::now();
		const double seconds = std::chrono::duration<double>(now - read_at).count();
		if (measured && seconds < 30)
			return info;
		if (!read)
		{
			info.os = osName();
			info.hardware = detectHardware();
			info.processor = detectProcessor();
			read = true;
		}
		const double cpu = cpuSeconds();
		if (seconds >= 1 && cpu >= cpu_at)
		{
			info.cpu = (float)(100.0 * (cpu - cpu_at) / seconds);
			read_at = now;
			cpu_at = cpu;
			measured = true;
		}
		info.memory = memoryInUse();
#if defined(__linux__) && !defined(__ANDROID__)
		std::ifstream route("/proc/net/route"), wireless("/proc/net/wireless");
		info.wifi = route && wireless && readWifi(route, wireless, info);
#endif
		return info;
	}

	std::vector<std::string> Helper::getFilesWithExtension(const std::string &directory, const std::string &extension)
	{
		std::vector<std::string> files;
		for (const std::string &name : getFilesInDirectory(directory))
		{
			if (name.size() < extension.size())
				continue;
#ifdef _WIN32
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

	bool Helper::isUUID(const std::string &s, bool lowercase)
	{
		if (s.size() != 36)
			return false;
		for (int i = 0; i < 36; i++)
		{
			const char c = s[i];
			if (i == 8 || i == 13 || i == 18 || i == 23)
			{
				if (c != '-')
					return false;
			}
			else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
					   (!lowercase && c >= 'A' && c <= 'F')))
				return false;
		}
		return true;
	}

	void Helper::lowerThreadPriority()
	{
#if defined(_WIN32)
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#elif defined(__APPLE__)
		pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(__linux__)
		setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 5);
#endif
	}

	std::string Helper::resolvePath(const std::string &f, const std::string &base)
	{
		std::string path = f;
		if (path.size() > 1 && path[0] == '~' && (path[1] == '/' || path[1] == '\\'))
		{
			const char *home = std::getenv("HOME");
			if (home && *home)
				path = std::string(home) + path.substr(1);
		}
		const bool absolute = (!path.empty() && (path[0] == '/' || path[0] == '\\')) ||
							  (path.size() > 2 && path[1] == ':' && (path[2] == '/' || path[2] == '\\'));
		if (path.empty() || absolute || base.empty())
			return path;
		return base + "/" + path;
	}
}
