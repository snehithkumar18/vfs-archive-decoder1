#include "../src/vfs.h"
#include <stdint.h>
#include <stddef.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    VFS vfs;
    
    // Mount archive: calls decompress_rle on compressed file entries
    if (vfs.mount_archive(data, size)) {
        // Access file to touch decompressed contents
        int fd = vfs.open_file("/file.bin");
        if (fd >= 0) {
            uint8_t buffer[1024];
            vfs.read_file(fd, buffer, sizeof(buffer));
        }
    }
    return 0;
}
