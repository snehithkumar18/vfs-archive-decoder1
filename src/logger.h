#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <iostream>
#include <fstream>

enum class LogLevel {
    DEBUG_LEVEL = 0,
    INFO_LEVEL = 1,
    WARN_LEVEL = 2,
    ERROR_LEVEL = 3
};

class VFSLogger {
private:
    std::mutex log_mutex;
    LogLevel current_level;
    uint64_t total_logs;
    
    // File logging and rotation
    std::ofstream log_file;
    std::string file_path;
    size_t max_file_size;
    int max_rotation_files;
    bool file_logging_enabled;

    VFSLogger();
    ~VFSLogger();

    void rotate_logs();
    void write_to_file(const std::string& text);

public:
    static VFSLogger& get_instance();

    void set_log_level(LogLevel level);
    LogLevel get_log_level() const;

    void enable_file_logging(const std::string& path, size_t max_size, int max_files);

    void log(LogLevel level, const std::string& sender, const std::string& message);
    void debug(const std::string& sender, const std::string& message);
    void info(const std::string& sender, const std::string& message);
    void warn(const std::string& sender, const std::string& message);
    void error(const std::string& sender, const std::string& message);

    uint64_t get_total_logs_count() const;
};

#endif // LOGGER_H
