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

#pragma once

#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <memory>
#include <atomic>

#include "Common.h"

enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERR,
    CRITICAL,
    EMPTY
};

struct LogMessage
{
    LogLevel level;
    std::string message;
    std::string time;
    uint32_t seq = 0;

    LogMessage() : level(LogLevel::EMPTY) {}
    LogMessage(LogLevel l, std::string msg, std::string time, uint32_t seq = 0) : level(l), message(std::move(msg)), time(std::move(time)), seq(seq) {}

    std::string levelToString() const;
    std::string toJSON() const;
};


class Logger : public Setting
{
public:
    static Logger &getInstance();

    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    typedef std::function<void(const LogMessage &)> LogCallback;
    void log(LogLevel level, const std::string &message);

    int addLogListener(LogCallback callback);
    void removeLogListener(int id);

    std::vector<LogMessage> getLastMessages(int n);
    void setMaxBufferSize(int size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_buffer_.clear();
        message_buffer_.resize(size);
        buffer_position_ = 0;
    }

    void setLogToSystem(std::string ident = "aiscatcher");

    void setMinLevel(LogLevel level) { min_level_ = level; }
    LogLevel getMinLevel() const { return min_level_; }

    Setting &SetKey(AIS::Keys key, const std::string &arg);

private:
    struct LogListener
    {
        int id;
        LogCallback callback;
    };

    Logger() : Setting("Logger") {}

    std::string getCurrentTime();

    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::INFO;
    std::atomic<uint32_t> seq_{1};

    std::vector<LogMessage> message_buffer_;
    int buffer_position_ = 0;
    std::vector<LogListener> log_listeners_;

    void storeMessage(const LogMessage &msg);
    void notifyListeners(const LogMessage &msg);

    int id = 1;
};

class LogStream
{
public:
    LogStream(LogLevel level);
    ~LogStream();

    LogStream(const LogStream &) = delete;
    LogStream &operator=(const LogStream &) = delete;

    LogStream(LogStream &&) = default;
    LogStream &operator=(LogStream &&) = default;

    template <typename T>
    LogStream &operator<<(const T &msg)
    {
        if (stream_) (*stream_) << msg;
        return *this;
    }

    typedef std::ostream &(*Manipulator)(std::ostream &);
    LogStream &operator<<(Manipulator manip)
    {
        if (stream_) manip(*stream_);
        return *this;
    }

private:
    LogLevel level_;
    std::unique_ptr<std::ostringstream> stream_;
};

LogStream Debug();
LogStream Info();
LogStream Warning();
LogStream Error();
LogStream Critical();
