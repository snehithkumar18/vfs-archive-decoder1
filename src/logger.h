#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <iostream>
#include <fstream>

namespace PixelForge {

enum class LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
};

class Logger {
private:
    std::mutex log_mutex;
    LogLevel current_level;
    uint64_t total_logs;
    
    std::ofstream log_file;
    std::string file_path;
    size_t max_file_size;
    int max_rotation_files;
    bool file_logging_enabled;

    Logger();
    ~Logger();

    void rotate_logs();
    void write_to_file(const std::string& text);

public:
    static Logger& getInstance();
    static Logger& get_instance() { return getInstance(); }

    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    void enableFileLogging(const std::string& path, size_t max_size, int max_files);

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    // Overloads with sender argument for backward compatibility
    void debug(const std::string& sender, const std::string& message);
    void info(const std::string& sender, const std::string& message);
    void warn(const std::string& sender, const std::string& message);
    void error(const std::string& sender, const std::string& message);

    uint64_t getTotalLogsCount() const;
};

} // namespace PixelForge

using VFSLogger = PixelForge::Logger;

#endif // LOGGER_H
