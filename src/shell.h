#ifndef SHELL_H
#define SHELL_H

#include "vfs.h"
#include <string>
#include <vector>

class VFSShell {
private:
    VFS& vfs;
    std::string current_dir;
    
    std::vector<std::string> tokenise(const std::string& cmd_line);

public:
    VFSShell(VFS& vfs_instance);
    
    std::string execute_command(const std::string& cmd_line);
    std::string get_current_working_directory() const;
};

#endif // SHELL_H
