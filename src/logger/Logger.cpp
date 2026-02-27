#include "logger/Logger.h"

#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>

Logger::Logger(const std::string& logFile, LogLevel minLevel)
    : minLevel_(minLevel)
{
    logFile_.open(logFile, std::ios::out | std::ios::trunc);
    if (!logFile_.is_open()) {
        std::cerr << "[Logger] WARNING: Could not open log file: " << logFile << std::endl;
    }
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message)  { log(LogLevel::INFO, message); }
void Logger::warn(const std::string& message)  { log(LogLevel::WARN, message); }
void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }

void Logger::log(LogLevel level, const std::string& message) {
    if (level < minLevel_) return;

    std::string formatted = "[" + timestamp() + "] [" + levelStr(level) + "] "
                          + "[" + threadId() + "] " + message;

    std::lock_guard<std::mutex> lock(mutex_);

    // Write to console
    std::cout << formatted << std::endl;

    // Write to file
    if (logFile_.is_open()) {
        logFile_ << formatted << std::endl;
        logFile_.flush();
    }
}

std::string Logger::levelStr(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::threadId() const {
    std::ostringstream oss;
    oss << "Thread-" << std::this_thread::get_id();
    return oss.str();
}
