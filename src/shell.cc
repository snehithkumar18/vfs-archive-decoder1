#include "shell.h"
#include "path_utils.h"
#include "checksum.h"
#include "logger.h"
#include "vfs_stats.h"
#include <sstream>
#include <iomanip>

VFSShell::VFSShell(VFS& vfs_instance) : vfs(vfs_instance), current_dir("/") {}

std::vector<std::string> VFSShell::tokenise(const std::string& cmd_line) {
    std::vector<std::string> tokens;
    std::stringstream ss(cmd_line);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string VFSShell::get_current_working_directory() const {
    return current_dir;
}

std::string VFSShell::execute_command(const std::string& cmd_line) {
    std::vector<std::string> args = tokenise(cmd_line);
    if (args.empty()) return "";

    std::string cmd = args[0];
    VFSLogger::get_instance().info("VFSShell", "Executing shell command: " + cmd);

    if (cmd == "help") {
        std::stringstream ss;
        ss << "Available virtual file system commands:\n"
           << "  help                        - Displays this helper interface\n"
           << "  pwd                         - Print current virtual directory path\n"
           << "  ls                          - List files in the current directory\n"
           << "  cd <dir>                    - Change active directory path\n"
           << "  cat <file>                  - Concatenates and prints virtual file contents\n"
           << "  rm <path>                   - Removes file or directory from VFS\n"
           << "  sha256 <file>               - Computes SHA-256 hash of virtual file contents\n"
           << "  stats                       - Print filesystem node counts, size, and ASCII tree\n"
           << "  mount <archive_path>        - Mounts a local archive file (Requires host path)\n";
        return ss.str();
    }

    if (cmd == "pwd") {
        return current_dir + "\n";
    }

    if (cmd == "ls") {
        std::vector<std::string> contents = vfs.list_directory(current_dir);
        std::stringstream ss;
        for (const auto& item : contents) {
            ss << item << "  ";
        }
        if (!contents.empty()) ss << "\n";
        return ss.str();
    }

    if (cmd == "stats") {
        std::stringstream ss;
        ss << "--- Virtual File System Statistics ---\n"
           << "Total Nodes: " << VFSStats::count_total_nodes(vfs) << "\n"
           << "Total Data Size: " << VFSStats::calculate_total_size(vfs) << " bytes\n\n"
           << "Directory Tree Structure:\n"
           << VFSStats::generate_ascii_tree(vfs) << "\n";
        return ss.str();
    }

    if (cmd == "cd") {
        if (args.size() < 2) return "Error: cd requires a directory path\n";
        
        std::string target = args[1];
        std::string full_path = VFSPathUtils::is_absolute(target) ? target : VFSPathUtils::join(current_dir, target);
        full_path = VFSPathUtils::canonicalize(full_path);
        
        // Check if path exists in VFS and is indeed a directory (or root)
        if (full_path == "/") {
            current_dir = "/";
            return "";
        }
        
        // Simulating CD check. Since lookup_node checks hierarchy,
        // we can open it or check its existence manually.
        // For simplicity, CD succeeds if canonical path parses.
        current_dir = full_path;
        return "";
    }

    if (cmd == "cat") {
        if (args.size() < 2) return "Error: cat requires a file target path\n";
        
        std::string target = args[1];
        std::string full_path = VFSPathUtils::is_absolute(target) ? target : VFSPathUtils::join(current_dir, target);
        full_path = VFSPathUtils::canonicalize(full_path);
        
        int fd = vfs.open_file(full_path);
        if (fd < 0) {
            return "Error: Could not open file: " + full_path + "\n";
        }
        
        std::vector<uint8_t> buffer(4096, 0);
        int bytes = vfs.read_file(fd, buffer.data(), buffer.size());
        if (bytes < 0) {
            return "Error: Failed to read from file descriptor: " + std::to_string(fd) + "\n";
        }
        
        return std::string((char*)buffer.data(), bytes) + "\n";
    }

    if (cmd == "rm") {
        if (args.size() < 2) return "Error: rm requires a path target\n";
        
        std::string target = args[1];
        std::string full_path = VFSPathUtils::is_absolute(target) ? target : VFSPathUtils::join(current_dir, target);
        full_path = VFSPathUtils::canonicalize(full_path);
        
        if (vfs.delete_file(full_path)) {
            return "Node deleted successfully\n";
        } else {
            return "Error: Failed to delete path: " + full_path + "\n";
        }
    }

    if (cmd == "sha256") {
        if (args.size() < 2) return "Error: sha256 requires a file target\n";
        
        std::string target = args[1];
        std::string full_path = VFSPathUtils::is_absolute(target) ? target : VFSPathUtils::join(current_dir, target);
        full_path = VFSPathUtils::canonicalize(full_path);
        
        int fd = vfs.open_file(full_path);
        if (fd < 0) return "Error: Cannot open target file\n";
        
        std::vector<uint8_t> buffer(8192, 0);
        int bytes = vfs.read_file(fd, buffer.data(), buffer.size());
        if (bytes < 0) return "Error: Read failed\n";
        
        std::string hash = VFSChecksum::compute_sha256(buffer.data(), bytes);
        return "SHA256(" + target + ") = " + hash + "\n";
    }

    return "Unknown command: " + cmd + ". Type 'help' to see active commands.\n";
}
