#include "../src/vfs.h"
#include "../src/shell.h"
#include <stdint.h>
#include <stddef.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    
    VFS vfs;
    VFSShell shell(vfs);
    
    // Construct command line string from raw fuzzer payload
    std::string cmd(reinterpret_cast<const char*>(data), size);
    
    // Process input in shell interpreter
    shell.execute_command(cmd);
    
    return 0;
}
