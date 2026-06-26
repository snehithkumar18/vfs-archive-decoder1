#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>

VFSLogger::VFSLogger() : current_level(LogLevel::INFO_LEVEL), total_logs(0), max_file_size(0), max_rotation_files(0), file_logging_enabled(false) {}

VFSLogger::~VFSLogger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

VFSLogger& VFSLogger::get_instance() {
    static VFSLogger instance;
    return instance;
}

void VFSLogger::set_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex);
    current_level = level;
}

LogLevel VFSLogger::get_log_level() const {
    return current_level;
}

uint64_t VFSLogger::get_total_logs_count() const {
    return total_logs;
}

void VFSLogger::enable_file_logging(const std::string& path, size_t max_size, int max_files) {
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

void VFSLogger::write_to_file(const std::string& text) {
    if (!file_logging_enabled || !log_file.is_open()) return;
    
    log_file << text << std::endl;
    log_file.flush();
    
    // Check file size using struct stat
    struct stat stat_buf;
    int rc = stat(file_path.c_str(), &stat_buf);
    if (rc == 0 && (size_t)stat_buf.st_size >= max_file_size) {
        rotate_logs();
    }
}

void VFSLogger::rotate_logs() {
    log_file.close();
    
    // Shift old log files: e.g. log.4 -> log.5, log.3 -> log.4 ...
    for (int i = max_rotation_files - 1; i >= 1; --i) {
        std::string old_name = file_path + "." + std::to_string(i);
        std::string new_name = file_path + "." + std::to_string(i + 1);
        std::rename(old_name.c_str(), new_name.c_str());
    }
    
    // Rename current log file to log.1
    std::string backup_name = file_path + ".1";
    std::rename(file_path.c_str(), backup_name.c_str());
    
    // Open new log file
    log_file.open(file_path, std::ios::out | std::ios::trunc);
    if (!log_file.is_open()) {
        file_logging_enabled = false;
    }
}

void VFSLogger::log(LogLevel level, const std::string& sender, const std::string& message) {
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
        case LogLevel::DEBUG_LEVEL:
            level_str = "DEBUG";
            color_code = "\033[36m"; // Cyan
            break;
        case LogLevel::INFO_LEVEL:
            level_str = "INFO";
            color_code = "\033[32m"; // Green
            break;
        case LogLevel::WARN_LEVEL:
            level_str = "WARN";
            color_code = "\033[33m"; // Yellow
            break;
        case LogLevel::ERROR_LEVEL:
            level_str = "ERROR";
            color_code = "\033[31m"; // Red
            break;
    }

    std::string reset_code = "\033[0m";
    
    // Formatted line
    std::string raw_log_line = "[" + time_ss.str() + "] [" + level_str + "] [" + sender + "] " + message;

    // Output to console
    std::cout << color_code << raw_log_line << reset_code << std::endl;
    
    // Output to file if enabled (bypassing mutex lock since we are already locked here)
    if (file_logging_enabled && log_file.is_open()) {
        log_file << raw_log_line << std::endl;
        log_file.flush();
        
        struct stat stat_buf;
        int rc = stat(file_path.c_str(), &stat_buf);
        if (rc == 0 && (size_t)stat_buf.st_size >= max_file_size) {
            // Inlined rotation logic to avoid deadlocking on mutex
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

void VFSLogger::debug(const std::string& sender, const std::string& message) {
    log(LogLevel::DEBUG_LEVEL, sender, message);
}

void VFSLogger::info(const std::string& sender, const std::string& message) {
    log(LogLevel::INFO_LEVEL, sender, message);
}

void VFSLogger::warn(const std::string& sender, const std::string& message) {
    log(LogLevel::WARN_LEVEL, sender, message);
}

void VFSLogger::error(const std::string& sender, const std::string& message) {
    log(LogLevel::ERROR_LEVEL, sender, message);
}
