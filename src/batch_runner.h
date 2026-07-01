#pragma once

#include "manifest.h"
#include "image.h"
#include "errors.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace PixelForge {

// Status of a batch processing job
enum class JobStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Skipped,
    Cancelled
};

inline const char* job_status_string(JobStatus status) {
    switch (status) {
        case JobStatus::Pending:    return "Pending";
        case JobStatus::Running:    return "Running";
        case JobStatus::Completed:  return "Completed";
        case JobStatus::Failed:     return "Failed";
        case JobStatus::Skipped:    return "Skipped";
        case JobStatus::Cancelled:  return "Cancelled";
        default:                    return "Unknown";
    }
}

// Result of executing a single batch job
struct JobResult {
    JobStatus   status          = JobStatus::Pending;
    std::string error_message;
    double      elapsed_seconds = 0.0;
    size_t      memory_used     = 0;
    std::string job_name;
    int         job_index       = -1;
};

// Progress callback: (current_job_index, total_jobs, job_name)
using ProgressCallback = std::function<void(int current, int total, const std::string& job_name)>;

// Operation function: takes the current working image and job params, returns
// a new image (caller takes ownership). Returns nullptr on failure and sets error.
using OperationFunc = std::function<Image*(Image* current, const ManifestJob& job, std::string& error)>;

// Executes batch processing jobs described by a Manifest.
class BatchRunner {
public:
    explicit BatchRunner(const Manifest& manifest);
    ~BatchRunner() = default;

    // Execute all jobs. Returns one result per job.
    std::vector<JobResult> run();

    // Validate all jobs without executing. Returns one result per job with
    // status Completed (valid) or Failed (invalid).
    std::vector<JobResult> dry_run();

    // Set a callback invoked before each job begins.
    void set_progress_callback(ProgressCallback cb);

    // Signal cancellation. Safe to call from another thread.
    void cancel();

    // Check if cancellation was requested.
    bool is_cancelled() const;

    // Execute a single job. The image pipeline: load -> op1 -> op2 -> ... -> save.
    JobResult run_single_job(const ManifestJob& job, int index);

    // Register a custom operation by name
    void register_operation(const std::string& name, OperationFunc func);

    // Get a summary report string after a run.
    static std::string format_summary(const std::vector<JobResult>& results);

private:
    // Initialize the default operation dispatch table
    void init_operations();

    // Resolve paths relative to the manifest base_dir
    std::string resolve_path(const std::string& path) const;

    // Extract a numeric parameter from a job, checking both "name" and "prefix_name" keys
    double get_param(const ManifestJob& job, const std::string& name, double fallback) const;
    std::string get_param_str(const ManifestJob& job, const std::string& name, const std::string& fallback) const;

    // Load an image from file path (auto-detect format from extension)
    Image* load_image(const std::string& path, std::string& error) const;

    // Save an image to file path (auto-detect format from extension)
    bool save_image(const Image* img, const std::string& path, std::string& error) const;

    // Detect image format from file extension
    std::string detect_format(const std::string& path) const;

    const Manifest& manifest_;
    ProgressCallback progress_cb_;
    std::atomic<bool> cancelled_{false};
    std::map<std::string, OperationFunc> operations_;
};

} // namespace PixelForge
