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


#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdio>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "Logger.h"
#include "Convert.h"
#include "Writer.h"


#ifdef HASSYSLOG
#include <syslog.h>
class SyslogHandler
{
	static std::string ident_;
public:
	SyslogHandler(const std::string &ident = "aiscatcher")
	{
		ident_ = ident;
		openlog(ident_.c_str(), LOG_PID, LOG_USER);
	}

	~SyslogHandler()
	{
	}

	void operator()(const LogMessage &msg)
	{
		int priority;
		switch (msg.level)
		{
		case LogLevel::DEBUG:
			priority = LOG_DEBUG;
			break;
		case LogLevel::ERR:
			priority = LOG_ERR;
			break;
		case LogLevel::WARNING:
			priority = LOG_WARNING;
			break;
		case LogLevel::CRITICAL:
			priority = LOG_CRIT;
			break;
		default:
			priority = LOG_INFO;
			break;
		}
		syslog(priority, "%s", msg.message.c_str());
	}
};
std::string SyslogHandler::ident_;
#else
class SyslogHandler
{
public:
	SyslogHandler(std::string ident)
	{
		std::cerr << "No system logger available" << std::endl;
		throw std::runtime_error("No system logger available");
	}

	~SyslogHandler()
	{
	}

	void operator()(const LogMessage &msg)
	{
	}
};
#endif

std::string LogMessage::levelToString() const
{
	switch (level)
	{
	case LogLevel::DEBUG:
		return "debug";
	case LogLevel::INFO:
		return "info";
	case LogLevel::WARNING:
		return "warning";
	case LogLevel::ERR:
		return "error";
	case LogLevel::CRITICAL:
		return "critical";
	case LogLevel::EMPTY:
		return "empty";
	}
	return "unknown";
}

std::string LogMessage::toJSON() const
{
	std::string msg = JSON::Writer::escape(message);

	return "{\"level\":\"" + levelToString() + "\",\"message\":" + msg + ",\"time\":\"" + time + "\",\"seq\":" + std::to_string(seq) + "}";
}

Logger &Logger::getInstance()
{
	static Logger instance;
	return instance;
}

Setting &Logger::SetKey(AIS::Keys key, const std::string &arg)
{
	switch (key)
	{
	case AIS::KEY_SETTING_SYSTEM:
		setLogToSystem("aiscatcher");
		break;
	case AIS::KEY_SETTING_LEVEL:
	{
		std::string a = arg;
		Util::Convert::toUpper(a);
		if (a == "DEBUG")
			min_level_ = LogLevel::DEBUG;
		else if (a == "INFO")
			min_level_ = LogLevel::INFO;
		else if (a == "WARNING")
			min_level_ = LogLevel::WARNING;
		else if (a == "ERROR")
			min_level_ = LogLevel::ERR;
		else if (a == "CRITICAL")
			min_level_ = LogLevel::CRITICAL;
		else
			throw std::runtime_error("Invalid log level: " + arg + ". Valid values: DEBUG, INFO, WARNING, ERROR, CRITICAL");
		break;
	}
	default:
		throw std::runtime_error("Unknown option.");
	}
	return *this;
}

void Logger::setLogToSystem(std::string ident)
{
	Logger::getInstance().addLogListener(SyslogHandler(ident));
}

std::string Logger::getCurrentTime()
{
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

void Logger::storeMessage(const LogMessage &msg)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (!message_buffer_.size())
		return;

	message_buffer_[buffer_position_] = msg;
	buffer_position_ = (buffer_position_ + 1) % message_buffer_.size();
}

std::vector<LogMessage> Logger::getLastMessages(int n)
{
	std::lock_guard<std::mutex> lock(mutex_);

	n = MIN(n, (int)message_buffer_.size());

	std::vector<LogMessage> result;
	if (n <= 0 || message_buffer_.empty())
	{
		return result;
	}

	int ptr = (buffer_position_ + message_buffer_.size() - n) % message_buffer_.size();

	for (int i = 0; i < n; i++)
	{
		if (message_buffer_[ptr].level != LogLevel::EMPTY)
		{
			result.push_back(message_buffer_[ptr]);
		}
		ptr = (ptr + 1) % message_buffer_.size();
	}

	return result;
}

int Logger::addLogListener(LogCallback callback)
{
	std::lock_guard<std::mutex> lock(mutex_);

	id++;
	log_listeners_.push_back({id, callback});
	return id;
}

void Logger::removeLogListener(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = std::find_if(log_listeners_.begin(), log_listeners_.end(),
						   [id](const LogListener &listener)
						   {
							   return listener.id == id;
						   });

	if (it != log_listeners_.end())
	{
		log_listeners_.erase(it);
	}
}

void Logger::notifyListeners(const LogMessage &msg)
{
	// Prevent unbounded recursion if a callback itself calls log().
	thread_local bool in_notify = false;
	if (in_notify) return;

	std::vector<LogListener> snapshot;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		snapshot = log_listeners_;
	}

	in_notify = true;
	try
	{
		for (const auto &listener : snapshot)
		{
			listener.callback(msg);
		}
	}
	catch (...)
	{
		in_notify = false;
		throw;
	}
	in_notify = false;
}

void Logger::log(LogLevel level, const std::string &message)
{
	if (level < min_level_)
		return;

	std::string time_str = getCurrentTime();
	LogMessage log_msg(level, message, time_str, seq_.fetch_add(1));

	storeMessage(log_msg);
	notifyListeners(log_msg);
}

LogStream::LogStream(LogLevel level)
	: level_(level)
{
	if (level_ >= Logger::getInstance().getMinLevel())
		stream_.reset(new std::ostringstream());
}

LogStream::~LogStream()
{
	if (stream_)
		Logger::getInstance().log(level_, stream_->str());
}

// Convenience functions
LogStream Debug()
{
	return LogStream(LogLevel::DEBUG);
}

LogStream Info()
{
	return LogStream(LogLevel::INFO);
}

LogStream Warning()
{
	return LogStream(LogLevel::WARNING);
}

LogStream Error()
{
	return LogStream(LogLevel::ERR);
}

LogStream Critical()
{
	return LogStream(LogLevel::CRITICAL);
}