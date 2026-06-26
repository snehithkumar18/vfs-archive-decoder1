#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>

namespace PixelForge {

Logger::Logger() : current_level(LogLevel::LOG_INFO), total_logs(0), max_file_size(0), max_rotation_files(0), file_logging_enabled(false) {}

Logger::~Logger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex);
    current_level = level;
}

LogLevel Logger::getLogLevel() const {
    return current_level;
}

uint64_t Logger::getTotalLogsCount() const {
    return total_logs;
}

void Logger::enableFileLogging(const std::string& path, size_t max_size, int max_files) {
    std::lock_guard<std::mutex> lock(log_mutex);
    file_path = path;
    max_file_size = max_size;
    max_rotation_files = max_files;
    file_logging_enabled = true;
    
    log_file.open(file_path, std::ios::out | std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << file_path << std::endl;
        file_logging_enabled = false;
    }
}

void Logger::write_to_file(const std::string& text) {
    if (!file_logging_enabled || !log_file.is_open()) return;
    
    log_file << text << std::endl;
    log_file.flush();
    
    struct stat stat_buf;
    int rc = stat(file_path.c_str(), &stat_buf);
    if (rc == 0 && (size_t)stat_buf.st_size >= max_file_size) {
        rotate_logs();
    }
}

void Logger::rotate_logs() {
    log_file.close();
    
    for (int i = max_rotation_files - 1; i >= 1; --i) {
        std::string old_name = file_path + "." + std::to_string(i);
        std::string new_name = file_path + "." + std::to_string(i + 1);
        std::rename(old_name.c_str(), new_name.c_str());
    }
    
    std::string backup_name = file_path + ".1";
    std::rename(file_path.c_str(), backup_name.c_str());
    
    log_file.open(file_path, std::ios::out | std::ios::trunc);
    if (!log_file.is_open()) {
        file_logging_enabled = false;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    if (level < current_level) return;
    total_logs++;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") 
            << '.' << std::setfill('0') << std::setw(3) << millis;

    std::string level_str;
    std::string color_code;

    switch (level) {
        case LogLevel::LOG_DEBUG:
            level_str = "DEBUG";
            color_code = "\033[36m"; // Cyan
            break;
        case LogLevel::LOG_INFO:
            level_str = "INFO";
            color_code = "\033[32m"; // Green
            break;
        case LogLevel::LOG_WARN:
            level_str = "WARN";
            color_code = "\033[33m"; // Yellow
            break;
        case LogLevel::LOG_ERROR:
            level_str = "ERROR";
            color_code = "\033[31m"; // Red
            break;
    }

    std::string reset_code = "\033[0m";
    
    std::string raw_log_line = "[" + time_ss.str() + "] [" + level_str + "] " + message;

    std::cout << color_code << raw_log_line << reset_code << std::endl;
    
    if (file_logging_enabled && log_file.is_open()) {
        log_file << raw_log_line << std::endl;
        log_file.flush();
        
        struct stat stat_buf;
        int rc = stat(file_path.c_str(), &stat_buf);
        if (rc == 0 && (size_t)stat_buf.st_size >= max_file_size) {
            log_file.close();
            for (int i = max_rotation_files - 1; i >= 1; --i) {
                std::string old_name = file_path + "." + std::to_string(i);
                std::string new_name = file_path + "." + std::to_string(i + 1);
                std::rename(old_name.c_str(), new_name.c_str());
            }
            std::string backup_name = file_path + ".1";
            std::rename(file_path.c_str(), backup_name.c_str());
            log_file.open(file_path, std::ios::out | std::ios::trunc);
        }
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::LOG_DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::LOG_INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::LOG_WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::LOG_ERROR, message);
}

void Logger::debug(const std::string& sender, const std::string& message) {
    log(LogLevel::LOG_DEBUG, "[" + sender + "] " + message);
}

void Logger::info(const std::string& sender, const std::string& message) {
    log(LogLevel::LOG_INFO, "[" + sender + "] " + message);
}

void Logger::warn(const std::string& sender, const std::string& message) {
    log(LogLevel::LOG_WARN, "[" + sender + "] " + message);
}

void Logger::error(const std::string& sender, const std::string& message) {
    log(LogLevel::LOG_ERROR, "[" + sender + "] " + message);
}

} // namespace PixelForge
