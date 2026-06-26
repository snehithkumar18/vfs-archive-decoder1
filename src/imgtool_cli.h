#ifndef PIXELFORGE_CLI_H
#define PIXELFORGE_CLI_H

#include "filter.h"
#include "metadata.h"
#include <string>
#include <vector>

namespace PixelForge {

class PixelForgeCLI {
private:
    Image* current_image = nullptr;
    FilterCache cache;
    std::vector<MetadataBlock*> current_metadata;

    std::vector<std::string> tokenise(const std::string& cmd_line);
    bool load_ppm(const std::string& path);
    bool save_ppm(const std::string& path, const Image* img);

public:
    PixelForgeCLI();
    ~PixelForgeCLI();

    std::string execute_command(const std::string& cmd_line);
    
    // Accessors for debugging/harnesses
    const Image* get_current_image() const { return current_image; }
    const FilterCache& get_cache() const { return cache; }
    const std::vector<MetadataBlock*>& get_metadata() const { return current_metadata; }
};

} // namespace PixelForge

#endif // PIXELFORGE_CLI_H
