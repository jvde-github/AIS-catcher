#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "Logger.h"

std::unique_ptr<Logger> Logger::instance_ = nullptr;

Logger& Logger::getInstance(const std::string& filename, bool log_to_console, bool log_to_file) {
	static std::once_flag onceFlag;
	std::call_once(onceFlag, [&]() {
		instance_.reset(new Logger(filename, log_to_console, log_to_file));
	});
	return *instance_;
}

Logger::Logger(const std::string& filename, bool log_to_console, bool log_to_file)
	: log_file_name_(filename), log_to_console_(log_to_console), log_to_file_(log_to_file) {
	if (log_to_file_) {
		// Changed from append mode to overwrite mode
		file_stream_.open(log_file_name_, std::ios::out | std::ios::binary);
		if (!file_stream_) {
			std::cerr << "Logger Error: Failed to open log file: " << log_file_name_ << std::endl;
			log_to_file_ = false; // Disable file logging if opening fails
		}
	}
}

Logger::~Logger() {
	if (file_stream_.is_open()) {
		file_stream_.close();
	}
}

std::string Logger::logLevelToString(LogLevel level) {
	switch (level) {
	case LogLevel::INFO:
		return "INFO";
	case LogLevel::WARNING:
		return "WARNING";
	case LogLevel::ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}

std::string Logger::getCurrentTime() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
	char time_buffer[20];

#ifdef _WIN32
	tm local_tm;
	localtime_s(&local_tm, &now_time_t);
	std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
#else
	tm local_tm;
	localtime_r(&now_time_t, &local_tm);
	std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
#endif

	// Get milliseconds
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	std::ostringstream ss;
	ss << time_buffer << "." << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

std::size_t Logger::getFileSize(const std::string& filename) {
#ifdef _WIN32
	struct _stat64 st;
	if (_stat64(filename.c_str(), &st) == 0) {
		return static_cast<std::size_t>(st.st_size);
	}
#else
	struct stat st;
	if (stat(filename.c_str(), &st) == 0) {
		return static_cast<std::size_t>(st.st_size);
	}
#endif
	return 0;
}

// Truncate the log file to retain the last kRetainSize bytes (5MB)
void Logger::truncateLogFile() {
	if (!log_to_file_) return;

	if (file_stream_.is_open()) {
		file_stream_.close();
	}

	std::size_t current_size = getFileSize(log_file_name_);
	std::size_t new_size = (current_size > kRetainSize) ? kRetainSize : current_size;
	std::size_t start_pos = current_size - new_size;

	std::vector<char> buffer(new_size);

#ifdef _WIN32
	// Open the file in binary mode
	FILE* file = nullptr;
	errno_t err = fopen_s(&file, log_file_name_.c_str(), "rb+");
	if (err != 0 || file == nullptr) {
		std::cerr << "Logger Error: Failed to open log file for truncation: " << log_file_name_ << std::endl;
		return;
	}

	// Read the last new_size bytes
	_fseeki64(file, start_pos, SEEK_SET);
	size_t bytes_read = fread(buffer.data(), 1, new_size, file);
	if (bytes_read != new_size) {
		std::cerr << "Logger Error: Failed to read log file during truncation: " << log_file_name_ << std::endl;
		fclose(file);
		return;
	}

	// Truncate the file
	_chsize_s(_fileno(file), 0);

	// Write back the retained data
	_fseeki64(file, 0, SEEK_SET);
	size_t bytes_written = fwrite(buffer.data(), 1, new_size, file);
	if (bytes_written != new_size) {
		std::cerr << "Logger Error: Failed to write to log file during truncation: " << log_file_name_ << std::endl;
	}

	fclose(file);
#else
	int fd = open(log_file_name_.c_str(), O_RDWR);
	if (fd == -1) {
		std::cerr << "Logger Error: Failed to open log file for truncation: " << log_file_name_ << std::endl;
		return;
	}

	// Read the last new_size bytes
	if (lseek(fd, start_pos, SEEK_SET) == -1) {
		std::cerr << "Logger Error: Failed to seek in log file: " << log_file_name_ << std::endl;
		close(fd);
		return;
	}
	ssize_t bytes_read = read(fd, buffer.data(), new_size);
	if (bytes_read != static_cast<ssize_t>(new_size)) {
		std::cerr << "Logger Error: Failed to read log file during truncation: " << log_file_name_ << std::endl;
		close(fd);
		return;
	}

	// Truncate the file
	if (ftruncate(fd, 0) != 0) {
		std::cerr << "Logger Error: Failed to truncate log file: " << log_file_name_ << std::endl;
		close(fd);
		return;
	}

	// Write back the retained data
	if (lseek(fd, 0, SEEK_SET) == -1) {
		std::cerr << "Logger Error: Failed to seek in log file: " << log_file_name_ << std::endl;
		close(fd);
		return;
	}
	ssize_t bytes_written = write(fd, buffer.data(), new_size);
	if (bytes_written != static_cast<ssize_t>(new_size)) {
		std::cerr << "Logger Error: Failed to write to log file during truncation: " << log_file_name_ << std::endl;
	}

	close(fd);
#endif

	// Reopen the log file for overwriting
	file_stream_.open(log_file_name_, std::ios::out | std::ios::binary);
	if (!file_stream_) {
		std::cerr << "Logger Error: Failed to reopen log file after truncation: " << log_file_name_ << std::endl;
		log_to_file_ = false;
	}
}

void Logger::rotateLogFile() {
	if (!log_to_file_ || !file_stream_.is_open()) return;

	std::size_t file_size = getFileSize(log_file_name_);
	if (file_size < kMaxFileSize) {
		return;
	}

	truncateLogFile();
}

void Logger::log(LogLevel level, const std::string& message) {

	if (log_to_file_) {
		std::string time_str = getCurrentTime();
		std::ostringstream formatted_message;
		formatted_message << "[" << logLevelToString(level) << "] [" << time_str << "] " << message << "\n";
		std::string final_message = formatted_message.str();

		{
			std::lock_guard<std::mutex> lock(mutex_);

			rotateLogFile();

			if (file_stream_.is_open()) {
				file_stream_ << final_message;
				file_stream_.flush();
			}
		}
	}

	if (log_to_console_) {

		if (level == LogLevel::INFO) {
			std::cerr << message << "\n";
		}
		else {
			std::cerr << logLevelToString(level) << ": " << message << "\n";
		}
	}
}


void Logger::setLogToConsole(bool enable) {
	std::lock_guard<std::mutex> lock(mutex_);
	log_to_console_ = enable;
}

void Logger::setLogToFile(bool enable, const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (enable && !log_to_file_) {
		// Enable file logging
		log_to_file_ = true;
		if (!filename.empty()) {
			log_file_name_ = filename;
		}
		// Open file stream if not open
		if (!file_stream_.is_open()) {
			file_stream_.open(log_file_name_, std::ios::out | std::ios::binary);
			if (!file_stream_) {
				std::cerr << "Logger Error: Failed to open log file: " << log_file_name_ << std::endl;
				log_to_file_ = false;
			}
		}
	}
	else if (!enable && log_to_file_) {
		// Disable file logging
		if (file_stream_.is_open()) {
			file_stream_.close();
		}
		log_to_file_ = false;
	}
}

void Logger::setLogToFile(bool enable) {
	setLogToFile(enable, "");
}

LogStream::LogStream(LogLevel level)
	: level_(level), stream_(new std::ostringstream()) {
}

LogStream::~LogStream() {
	Logger::getInstance().log(level_, stream_->str());
}

// Convenience functions
LogStream Info() {
	return LogStream(LogLevel::INFO);
}

LogStream Warning() {
	return LogStream(LogLevel::WARNING);
}

LogStream Error() {
	return LogStream(LogLevel::ERROR);
}
