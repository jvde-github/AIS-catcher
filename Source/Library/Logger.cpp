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
#include <cstdio>
#include <cstdlib>
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
	// openlog() keeps the pointer rather than copying, so the ident has to outlive us.
	static std::string ident_;

public:
	SyslogHandler(const std::string &ident = "aiscatcher")
	{
		ident_ = ident;
		openlog(ident_.c_str(), LOG_PID, LOG_USER);
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
		throw std::runtime_error("No system logger available");
	}

	void operator()(const LogMessage &msg) {}
};
#endif

// Indexed by LogLevel.
static const char *const LEVEL_NAMES[] = {"debug", "info", "warning", "error", "critical", "empty"};
static const int LEVEL_COUNT = (int)(sizeof(LEVEL_NAMES) / sizeof(*LEVEL_NAMES));

const char *LogMessage::levelToString() const
{
	int i = (int)level;
	return (i >= 0 && i < LEVEL_COUNT) ? LEVEL_NAMES[i] : "unknown";
}

int LogMessage::syslogLevel() const
{
	switch (level)
	{
	case LogLevel::DEBUG:
		return 7;
	case LogLevel::WARNING:
		return 4;
	case LogLevel::ERR:
		return 3;
	case LogLevel::CRITICAL:
		return 2;
	default:
		return 6;
	}
}

std::string LogMessage::toJSON() const
{
	return std::string("{\"level\":\"") + levelToString() +
		   "\",\"message\":" + JSON::Writer::escape(message) +
		   ",\"time\":\"" + time + "\",\"seq\":" + std::to_string(seq) + "}";
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

		for (int i = 0; i <= (int)LogLevel::CRITICAL; i++)
		{
			std::string name = LEVEL_NAMES[i];
			Util::Convert::toUpper(name);
			if (a == name)
			{
				min_level_ = (LogLevel)i;
				return *this;
			}
		}
		throw std::runtime_error("Invalid log level: " + arg + ". Valid values: DEBUG, INFO, WARNING, ERROR, CRITICAL");
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
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	tm local_tm;
#ifdef _WIN32
	localtime_s(&local_tm, &t);
#else
	localtime_r(&t, &local_tm);
#endif

	char buf[32];
	size_t n = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
	std::snprintf(buf + n, sizeof(buf) - n, ".%03d", (int)ms.count());
	return std::string(buf);
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

int Logger::addConsoleListener()
{
	// journald parses a <N> priority prefix on stderr lines and strips it
	// (sd-daemon(3)), so log levels survive the journey into the journal
	const bool journal = getenv("JOURNAL_STREAM") != nullptr;
	return addLogListener([journal](const LogMessage &msg)
						  {
							  if (journal)
								  std::cerr << '<' << msg.syslogLevel() << '>' << msg.message << "\n";
							  else
								  std::cerr << msg.message << "\n"; });
}

int Logger::addLogListener(LogCallback callback)
{
	std::lock_guard<std::mutex> lock(mutex_);

	log_listeners_.push_back({++id, callback});
	return id;
}

void Logger::removeLogListener(int id)
{
	std::lock_guard<std::mutex> lock(mutex_);

	log_listeners_.erase(std::remove_if(log_listeners_.begin(), log_listeners_.end(),
										[id](const LogListener &l)
										{ return l.id == id; }),
						 log_listeners_.end());
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

	struct Guard
	{
		bool &flag;
		Guard(bool &f) : flag(f) { flag = true; }
		~Guard() { flag = false; }
	} guard(in_notify);

	for (const auto &listener : snapshot)
		listener.callback(msg);
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