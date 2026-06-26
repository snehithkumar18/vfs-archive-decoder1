#include "../src/vfs.h"
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    
    VFS vfs;
    
    // Split input: first part mounts an archive, second part runs API commands
    size_t mount_size = size / 2;
    vfs.mount_archive(data, mount_size);
    
    size_t idx = mount_size;
    std::vector<int> fds;
    
    while (idx < size) {
        uint8_t op = data[idx++];
        switch (op % 4) {
            case 0: { // Open File
                std::string path = "/";
                uint8_t path_len = (idx < size) ? (data[idx++] % 10) : 3;
                for (uint8_t i = 0; i < path_len && idx < size; ++i) {
                    char c = (char)data[idx++];
                    if (c >= 'a' && c <= 'z') {
                        path += c;
                    }
                }
                // Conditionally append "double" to hit double-free cache eviction logic
                if (idx < size && (data[idx++] & 1)) {
                    path += "double";
                }
                
                int fd = vfs.open_file(path);
                if (fd >= 0) {
                    fds.push_back(fd);
                }
                break;
            }
            
            case 1: { // Read File (UAF and Type Confusion target)
                if (fds.empty()) break;
                size_t fd_idx = (idx < size) ? (data[idx++] % fds.size()) : 0;
                uint8_t buf[64];
                vfs.read_file(fds[fd_idx], buf, sizeof(buf));
                break;
            }
            
            case 2: { // Delete File (UAF source)
                std::string path = "/";
                uint8_t path_len = (idx < size) ? (data[idx++] % 10) : 3;
                for (uint8_t i = 0; i < path_len && idx < size; ++i) {
                    char c = (char)data[idx++];
                    if (c >= 'a' && c <= 'z') {
                        path += c;
                    }
                }
                if (idx < size && (data[idx++] & 1)) {
                    path += "double";
                }
                vfs.delete_file(path);
                break;
            }
            
            case 3: { // Clear Cache (Double free trigger)
                vfs.clear_cache();
                break;
            }
        }
    }
    
    return 0;
}
