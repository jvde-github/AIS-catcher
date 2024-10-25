#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>

// Log levels
enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance(const std::string& filename = "log.txt", bool log_to_console = false, bool log_to_file = false);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    ~Logger();

    void log(LogLevel level, const std::string& message);

    // Updated method declarations
    void setLogToConsole(bool enable);

    // Overloaded setLogToFile methods
    void setLogToFile(bool enable);
    void setLogToFile(bool enable, const std::string& filename);

private:
    Logger(const std::string& filename, bool log_to_console, bool log_to_file);

    void rotateLogFile();
    void truncateLogFile();
    std::string logLevelToString(LogLevel level);
    std::string getCurrentTime();
    std::size_t getFileSize(const std::string& filename);

    std::ofstream file_stream_;
    std::string log_file_name_;

    static constexpr std::size_t kMaxFileSize = 10 * 1024 * 1024;
    static constexpr std::size_t kRetainSize = 5 * 1024 * 1024;

    std::mutex mutex_;

    bool log_to_console_;
    bool log_to_file_;

    static std::unique_ptr<Logger> instance_;
};

class LogStream {
public:
    LogStream(LogLevel level);
    ~LogStream();

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    LogStream(LogStream&&) = default;
    LogStream& operator=(LogStream&&) = default;

    template <typename T>
    LogStream& operator<<(const T& msg) {
        (*stream_) << msg;
        return *this;
    }

    typedef std::ostream& (*Manipulator)(std::ostream&);
    LogStream& operator<<(Manipulator manip) {
        manip(*stream_);
        return *this;
    }

private:
    LogLevel level_;
    std::unique_ptr<std::ostringstream> stream_;
};

// Convenience functions
LogStream Info();
LogStream Warning();
LogStream Error();

#endif // LOGGER_H
