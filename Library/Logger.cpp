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
#include "Utilities.h"
#include "JSON/StringBuilder.h"

std::unique_ptr<Logger> Logger::instance_ = nullptr;

#ifdef HASSYSLOG
#include <syslog.h>
class SyslogHandler
{
public:
	SyslogHandler(std::string ident = "aiscatcher")
	{
		openlog(ident.c_str(), LOG_PID, LOG_USER);
	}

	~SyslogHandler()
	{
		closelog();
	}

	void operator()(const LogMessage &msg)
	{
		int priority;
		switch (msg.level)
		{
		case LogLevel::_ERROR:
			priority = LOG_ERR;
			break;
		case LogLevel::_WARNING:
			priority = LOG_WARNING;
			break;
		case LogLevel::_CRITICAL:
			priority = LOG_CRIT;
			break;
		default:
			priority = LOG_INFO;
			break;
		}
		syslog(priority, "%s", msg.message.c_str());
	}
};
#else
class SyslogHandler
{
public:
	SyslogHandler(std::string ident)
	{
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
	case LogLevel::_INFO:
		return "info";
	case LogLevel::_WARNING:
		return "warning";
	case LogLevel::_ERROR:
		return "error";
	case LogLevel::_CRITICAL:
		return "critical";
	case LogLevel::_COMMAND:
		return "command";
	case LogLevel::_EMPTY:
		return "empty";
	}
	return "unknown";
}

std::string LogMessage::toJSON() const
{
	std::string msg;
	JSON::StringBuilder::stringify(message, msg);

	return "{\"level\":\"" + levelToString() + "\",\"message\":" + msg + ",\"time\":\"" + time + "\"}";
}

Logger &Logger::getInstance()
{
	static std::once_flag onceFlag;
	std::call_once(onceFlag, [&]()
				   { instance_.reset(new Logger()); });
	return *instance_;
}

Setting &Logger::Set(std::string option, std::string arg)
{
	Util::Convert::toUpper(option);

	if (option == "SYSTEM")
	{
		setLogToSystem("aiscatcher");
	}
	else
	{
		throw std::runtime_error("Unknown option: " + option);
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

	n = MIN(n, message_buffer_.size());

	std::vector<LogMessage> result;
	if (!message_buffer_.size())
	{
		return result;
	}

	int ptr = (buffer_position_ + 1 + message_buffer_.size() - n) % message_buffer_.size();

	while (ptr != buffer_position_)
	{
		if (message_buffer_[ptr].level != LogLevel::_EMPTY)
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
	static std::atomic<bool> in_notify(false);
	bool is_notifying = in_notify.exchange(true);

	if (is_notifying)
		return;

	std::lock_guard<std::mutex> lock(mutex_);

	for (const auto &listener : log_listeners_)
	{
		listener.callback(msg);
	}
	in_notify.store(false);
}

void Logger::log(LogLevel level, const std::string &message)
{
	std::string time_str = getCurrentTime();
	LogMessage log_msg(level, message, time_str);

	storeMessage(log_msg);
	notifyListeners(log_msg);
}

LogStream::LogStream(LogLevel level)
	: level_(level), stream_(new std::ostringstream())
{
}

LogStream::~LogStream()
{
	Logger::getInstance().log(level_, stream_->str());
}

// Convenience functions
LogStream Info()
{
	return LogStream(LogLevel::_INFO);
}

LogStream Warning()
{
	return LogStream(LogLevel::_WARNING);
}

LogStream Error()
{
	return LogStream(LogLevel::_ERROR);
}

LogStream Critical()
{
	return LogStream(LogLevel::_CRITICAL);
}

LogStream Command()
{
	return LogStream(LogLevel::_COMMAND);
}