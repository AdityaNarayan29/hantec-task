#pragma once

#include <string>
#include <fstream>
#include <mutex>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

/// Thread-safe logger that writes to both console and a log file.
/// Uses a mutex to prevent interleaved output from multiple worker threads.
class Logger {
public:
    explicit Logger(const std::string& logFile = "deal_processor.log",
                    LogLevel minLevel = LogLevel::INFO);
    ~Logger();

    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    void log(LogLevel level, const std::string& message);

private:
    std::string levelStr(LogLevel level) const;
    std::string timestamp() const;
    std::string threadId() const;

    std::ofstream logFile_;
    LogLevel      minLevel_;
    std::mutex    mutex_;
};
