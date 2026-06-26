#include "../src/vfs.h"
#include <stdint.h>
#include <stddef.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    VFS vfs;
    
    // Trigger Bug 1 (Integer Overflow) and Bug 5 (Unterminated Name Read)
    vfs.mount_archive(data, size);
    
    // Trigger Bug 6 (Type Confusion)
    // If we opened a directory node renamed/crafted with ".raw", read_file casts it and crashes.
    int fd = vfs.open_file("/foo.raw");
    if (fd >= 0) {
        uint8_t buffer[64];
        vfs.read_file(fd, buffer, sizeof(buffer));
    }
    
    return 0;
}
