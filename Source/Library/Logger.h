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
    __DEBUG,
    __INFO,
    __WARNING,
    __ERROR,
    __CRITICAL,
    __EMPTY
};

struct LogMessage
{
    LogLevel level;
    std::string message;
    std::string time;

    LogMessage() : level(LogLevel::__EMPTY), message(std::move("")), time(std::move("")) {}
    LogMessage(LogLevel l, std::string msg, std::string time) : level(l), message(std::move(msg)), time(std::move(time)) {}

    std::string levelToString() const;
    std::string toJSON() const;
};


class Logger : public Setting
{
public:
    static Logger &getInstance();

    ~Logger() = default;

    typedef std::function<void(const LogMessage &)> LogCallback;
    void log(LogLevel level, const std::string &message);

    int addLogListener(LogCallback callback);
    void removeLogListener(int id);

    std::vector<LogMessage> getLastMessages(int n);
    void setMaxBufferSize(int size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_buffer_.resize(size);
    }

    void setLogToSystem(std::string ident = "aiscatcher");

    void setMinLevel(LogLevel level) { min_level_ = level; }
    LogLevel getMinLevel() const { return min_level_; }

    Setting &Set(std::string option, std::string arg);

private:
    struct LogListener
    {
        int id;
        LogCallback callback;
    };

    std::string getCurrentTime();

    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::__INFO;

    static std::unique_ptr<Logger> instance_;

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
        (*stream_) << msg;
        return *this;
    }

    typedef std::ostream &(*Manipulator)(std::ostream &);
    LogStream &operator<<(Manipulator manip)
    {
        manip(*stream_);
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
