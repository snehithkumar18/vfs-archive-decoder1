#include "../src/imgtool_cli.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    
    std::string data_str(reinterpret_cast<const char*>(data), size);
    std::istringstream iss(data_str);
    std::string line;
    
    PixelForge::PixelForgeCLI cli;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            cli.execute_command(line);
        }
    }
    
    return 0;
}
