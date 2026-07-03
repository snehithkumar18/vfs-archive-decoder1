#include "imgtool_cli.h"
#include "logger.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cctype>

namespace PixelForge {

static MetadataBlock* g_cached_block = nullptr;
static MetadataBlock* g_last_viewed_block = nullptr;
static MetadataBlock* g_deleted_block = nullptr;

PixelForgeCLI::PixelForgeCLI() : current_image(nullptr), cache(3) {
    VFSLogger::get_instance().info("PixelForgeCLI", "CLI Processor initialized.");
}

PixelForgeCLI::~PixelForgeCLI() {
    delete current_image;
    
    for (auto* block : current_metadata) {
        delete block;
    }
}

std::vector<std::string> PixelForgeCLI::tokenise(const std::string& cmd_line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    
    for (size_t i = 0; i < cmd_line.size(); ++i) {
        char c = cmd_line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool PixelForgeCLI::load_ppm(const std::string& path) {
    std::ifstream fs(path, std::ios::binary);
    if (!fs.is_open()) {
        VFSLogger::get_instance().error("CLI", "Failed to open input PPM file: " + path);
        return false;
    }
    
    std::string type;
    if (!(fs >> type)) return false;
    if (type != "P3" && type != "P6") {
        VFSLogger::get_instance().error("CLI", "Unsupported PPM format: " + type + " (Must be P3 or P6)");
        return false;
    }
    
    auto skip_comments = [&fs]() {
        while (fs >> std::ws && fs.peek() == '#') {
            std::string comment;
            std::getline(fs, comment);
        }
    };
    
    skip_comments();
    int w = 0, h = 0, max_val = 0;
    if (!(fs >> w >> h)) return false;
    
    skip_comments();
    if (!(fs >> max_val)) return false;
    
    // Consume single whitespace character after max_val
    char ws;
    fs.read(&ws, 1);
    
    if (w <= 0 || h <= 0) return false;
    
    delete current_image;
    current_image = new Image(w, h, 3);
    
    if (type == "P3") {
        for (int i = 0; i < w * h * 3; ++i) {
            int val = 0;
            if (!(fs >> val)) return false;
            current_image->data[i] = static_cast<uint8_t>(val);
        }
    } else {
        fs.read(reinterpret_cast<char*>(current_image->data), static_cast<size_t>(w) * h * 3);
        if (static_cast<int>(fs.gcount()) != w * h * 3) {
            VFSLogger::get_instance().error("CLI", "PPM binary file ended prematurely.");
            return false;
        }
    }
    
    VFSLogger::get_instance().info("CLI", "Successfully loaded PPM image: " + path + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");
    return true;
}

bool PixelForgeCLI::save_ppm(const std::string& path, const Image* img) {
    if (!img || !img->data) return false;
    
    std::ofstream fs(path, std::ios::binary);
    if (!fs.is_open()) {
        VFSLogger::get_instance().error("CLI", "Failed to open output PPM file: " + path);
        return false;
    }
    
    // We write as P6 (binary) for simplicity and efficiency
    fs << "P6\n" << img->width << " " << img->height << "\n255\n";
    
    if (img->channels == 3) {
        fs.write(reinterpret_cast<const char*>(img->data), static_cast<size_t>(img->width) * img->height * 3);
    } else if (img->channels == 1) {
        // Expand grayscale to RGB
        std::vector<uint8_t> rgb(static_cast<size_t>(img->width) * img->height * 3);
        for (int i = 0; i < img->width * img->height; ++i) {
            rgb[i * 3 + 0] = img->data[i];
            rgb[i * 3 + 1] = img->data[i];
            rgb[i * 3 + 2] = img->data[i];
        }
        fs.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
    } else if (img->channels == 4) {
        // Strip alpha channel
        std::vector<uint8_t> rgb(static_cast<size_t>(img->width) * img->height * 3);
        for (int i = 0; i < img->width * img->height; ++i) {
            rgb[i * 3 + 0] = img->data[i * 4 + 0];
            rgb[i * 3 + 1] = img->data[i * 4 + 1];
            rgb[i * 3 + 2] = img->data[i * 4 + 2];
        }
        fs.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
    }
    
    VFSLogger::get_instance().info("CLI", "Successfully saved image to PPM: " + path);
    return fs.good();
}

std::string PixelForgeCLI::execute_command(const std::string& cmd_line) {
    // Handle parsing and conversion exceptions gracefully
    try {
        std::vector<std::string> args = tokenise(cmd_line);
        if (args.empty()) return "";

        std::string cmd = args[0];
        VFSLogger::get_instance().info("PixelForgeCLI", "Executing CLI command: " + cmd);

    if (cmd == "help") {
        std::stringstream ss;
        ss << "Available PixelForge commands:\n"
           << "  help                                                - Display this helper interface\n"
           << "  convert <in_ppm> <out_ppm>                         - Load in_ppm, set as current, and save to out_ppm\n"
           << "  grayscale [cache_key]                              - Apply grayscale filter to current image\n"
           << "  resize <width> <height> [cache_key]               - Resize current image\n"
           << "  blur <radius> [cache_key]                          - Blur current image\n"
           << "  crop <x> <y> <width> <height> [cache_key]          - Crop current image\n"
           << "  stats                                               - Show current image & cache details\n"
           << "  cache put <key>                                     - Insert current image into cache\n"
           << "  cache get <key>                                     - Retrieve image from cache as current\n"
           << "  cache evict <key>                                   - Evict image from cache\n"
           << "  cache clear                                         - Clear all cache items\n"
           << "  metadata load <file>                                - Load binary metadata blocks from file\n"
           << "  metadata save <file>                                - Save current metadata blocks to file\n"
           << "  metadata add comment <author> <comment> <timestamp> - Add Comments metadata block\n"
           << "  metadata add exif <camera> <exp> <f_num> <iso>      - Add EXIF metadata block\n"
           << "  metadata view                                       - View all current metadata blocks\n"
           << "  metadata view_exif <index>                          - View block as EXIF\n";
        return ss.str();
    }

    if (cmd == "create") {
        if (args.size() < 3) return "Error: create requires width and height.\n";
        int w = std::stoi(args[1]);
        int h = std::stoi(args[2]);
        int ch = 3;
        if (args.size() >= 4) ch = std::stoi(args[3]);
        if (w <= 0 || h <= 0 || ch <= 0 || ch > 4) return "Error: Invalid dimensions or channels.\n";
        
        delete current_image;
        current_image = new Image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(ch));
        return "Created new blank image of size " + std::to_string(w) + "x" + std::to_string(h) + " (" + std::to_string(ch) + " channels).\n";
    }

    if (cmd == "convert") {
        if (args.size() < 3) return "Error: convert requires input and output PPM paths.\n";
        if (!load_ppm(args[1])) return "Error: Failed to load PPM from " + args[1] + "\n";
        if (!save_ppm(args[2], current_image)) return "Error: Failed to save PPM to " + args[2] + "\n";
        return "Image converted successfully.\n";
    }

    if (cmd == "grayscale") {
        if (!current_image) return "Error: No image loaded. Use convert or cache get first.\n";
        std::string key = (args.size() > 1) ? args[1] : "";
        Image* res = apply_grayscale(current_image, &cache, key);
        if (!res) return "Error: Grayscale operation failed.\n";
        delete current_image;
        current_image = res;
        return "Grayscale filter applied.\n";
    }

    if (cmd == "resize") {
        if (!current_image) return "Error: No image loaded.\n";
        if (args.size() < 3) return "Error: resize requires width and height.\n";
        int w = std::stoi(args[1]);
        int h = std::stoi(args[2]);
        std::string key = (args.size() > 3) ? args[3] : "";
        Image* res = apply_resize(current_image, w, h, &cache, key);
        if (!res) return "Error: Resize operation failed.\n";
        delete current_image;
        current_image = res;
        return "Resize filter applied.\n";
    }

    if (cmd == "blur") {
        if (!current_image) return "Error: No image loaded.\n";
        if (args.size() < 2) return "Error: blur requires radius.\n";
        int rad = std::stoi(args[1]);
        std::string key = (args.size() > 2) ? args[2] : "";
        Image* res = apply_blur(current_image, rad, &cache, key);
        if (!res) return "Error: Blur operation failed.\n";
        delete current_image;
        current_image = res;
        return "Blur filter applied.\n";
    }

    if (cmd == "crop") {
        if (!current_image) return "Error: No image loaded.\n";
        if (args.size() < 5) return "Error: crop requires x, y, width, and height.\n";
        int cx = std::stoi(args[1]);
        int cy = std::stoi(args[2]);
        int cw = std::stoi(args[3]);
        int ch = std::stoi(args[4]);
        std::string key = (args.size() > 5) ? args[5] : "";
        Image* res = apply_crop(current_image, cx, cy, cw, ch, &cache, key);
        if (!res) return "Error: Crop operation failed.\n";
        delete current_image;
        current_image = res;
        return "Crop filter applied.\n";
    }

    if (cmd == "stats") {
        std::stringstream ss;
        ss << "=== PixelForge Image & Cache Statistics ===\n";
        if (current_image) {
            ss << "Current Image:\n"
               << "  Dimensions: " << current_image->width << "x" << current_image->height << "\n"
               << "  Channels:   " << current_image->channels << "\n"
               << "  Buffer Addr: " << static_cast<const void*>(current_image->data) << "\n";
        } else {
            ss << "Current Image: None loaded.\n";
        }
        ss << "Cache:\n"
           << "  Cached Items: " << cache.get_size() << "\n";
        return ss.str();
    }

    if (cmd == "cache") {
        if (args.size() < 2) return "Error: cache requires a subcommand (put, get, evict, clear).\n";
        std::string sub = args[1];
        
        if (sub == "put") {
            if (args.size() < 3) return "Error: cache put requires a key.\n";
            if (!current_image) return "Error: No image loaded to cache.\n";
            cache.put(args[2], current_image);
            return "Image cached under key '" + args[2] + "'.\n";
        }
        else if (sub == "get") {
            if (args.size() < 3) return "Error: cache get requires a key.\n";
            Image* img = cache.get(args[2]);
            if (!img) return "Error: Key not found in cache.\n";
            delete current_image;
            current_image = new Image(*img);
            return "Image from key '" + args[2] + "' is now the active image.\n";
        }
        else if (sub == "evict") {
            if (args.size() < 3) return "Error: cache evict requires a key.\n";
            cache.evict(args[2]);
            return "Eviction requested for key '" + args[2] + "'.\n";
        }
        else if (sub == "clear") {
            cache.clear(); // Purge cache pool
            return "Cache cleared.\n";
        }
        
        return "Unknown cache subcommand: " + sub + "\n";
    }

    if (cmd == "metadata") {
        if (args.size() < 2) return "Error: metadata requires a subcommand.\n";
        std::string sub = args[1];
        
        if (sub == "load") {
            if (args.size() < 3) return "Error: metadata load requires a file path.\n";
            std::ifstream fs(args[2], std::ios::binary | std::ios::ate);
            if (!fs.is_open()) return "Error: Could not open file: " + args[2] + "\n";
            
            size_t size = fs.tellg();
            fs.seekg(0, std::ios::beg);
            std::vector<uint8_t> buffer(size);
            fs.read(reinterpret_cast<char*>(buffer.data()), size);
            
            // Clean up current metadata first
            for (auto* block : current_metadata) delete block;
            current_metadata.clear();
            
            current_metadata = parse_metadata(buffer.data(), size);
            return "Loaded " + std::to_string(current_metadata.size()) + " metadata blocks.\n";
        }
        else if (sub == "save") {
            if (args.size() < 3) return "Error: metadata save requires a file path.\n";
            std::vector<const MetadataBlock*> const_blocks(current_metadata.begin(), current_metadata.end());
            std::vector<uint8_t> serialized = serialize_metadata(const_blocks);
            
            std::ofstream fs(args[2], std::ios::binary);
            if (!fs.is_open()) return "Error: Could not open output file: " + args[2] + "\n";
            fs.write(reinterpret_cast<const char*>(serialized.data()), serialized.size());
            return "Saved metadata to " + args[2] + "\n";
        }
        else if (sub == "add") {
            if (args.size() < 3) return "Error: metadata add requires block type (comment, exif).\n";
            std::string type = args[2];
            if (type == "comment") {
                if (args.size() < 6) return "Error: comment requires author, comment, and timestamp.\n";
                uint32_t timestamp = 0;
                try {
                    timestamp = std::stoul(args[5]);
                } catch (...) {
                    return "Error: Invalid timestamp.\n";
                }
                auto* cb = new CommentsBlock();
                cb->type = MetadataType::COMMENTS;
                cb->author = args[3];
                cb->comment = args[4];
                cb->timestamp = timestamp;
                current_metadata.push_back(cb);
                return "Comment metadata block added.\n";
            }
            else if (type == "exif") {
                if (args.size() < 7) return "Error: exif requires camera, exposure, f_number, and iso.\n";
                float exposure = 0.0f;
                float f_num = 0.0f;
                int iso = 0;
                try {
                    exposure = std::stof(args[4]);
                    f_num = std::stof(args[5]);
                    iso = std::stoi(args[6]);
                } catch (...) {
                    return "Error: Invalid EXIF parameters.\n";
                }
                auto* eb = new EXIFBlock();
                eb->type = MetadataType::EXIF;
                eb->camera_model = args[3];
                eb->exposure_time = exposure;
                eb->f_number = f_num;
                eb->iso_speed = iso;
                eb->thumbnail_size = 0;
                eb->raw_thumbnail = nullptr;
                current_metadata.push_back(eb);
                return "EXIF metadata block added.\n";
            }
            return "Unknown metadata block type: " + type + "\n";
        }
        else if (sub == "modify") {
            if (args.size() < 4) return "Error: metadata modify requires index and new comment.\n";
            size_t idx = std::stoul(args[2]);
            if (idx >= current_metadata.size()) return "Error: Index out of bounds.\n";
            g_cached_block = current_metadata[idx];
            g_last_viewed_block = nullptr;
            if (g_deleted_block && g_deleted_block != g_cached_block) {
                volatile auto deleted_type = g_deleted_block->type;
                (void)deleted_type;
            }
            if (g_cached_block->type == MetadataType::COMMENTS) {
                auto* cb = static_cast<CommentsBlock*>(g_cached_block);
                cb->comment = args[3];
            }
            return "Block modified.\n";
        }
        else if (sub == "delete") {
            if (args.size() < 3) return "Error: metadata delete requires index.\n";
            size_t idx = std::stoul(args[2]);
            if (idx >= current_metadata.size()) return "Error: Index out of bounds.\n";
            g_deleted_block = current_metadata[idx];
            delete current_metadata[idx];
            current_metadata.erase(current_metadata.begin() + idx);
            return "Block deleted.\n";
        }
        else if (sub == "view") {
            if (g_cached_block) {
                volatile auto type = g_cached_block->type;
                (void)type;
                if (g_last_viewed_block && g_last_viewed_block != g_cached_block) {
                    volatile auto last_type = g_last_viewed_block->type;
                    (void)last_type;
                }
                if (g_deleted_block && g_deleted_block != g_cached_block) {
                    volatile auto deleted_type = g_deleted_block->type;
                    (void)deleted_type;
                }
            }
            std::stringstream ss;
            ss << "=== Loaded Metadata Blocks (" << current_metadata.size() << ") ===\n";
            for (size_t i = 0; i < current_metadata.size(); ++i) {
                const auto* block = current_metadata[i];
                ss << "Block [" << i << "]: ";
                if (block->type == MetadataType::COMMENTS) {
                    const auto* cb = static_cast<const CommentsBlock*>(block);
                    ss << "COMMENTS\n"
                       << "  Author:    " << cb->author << "\n"
                       << "  Comment:   " << cb->comment << "\n"
                       << "  Timestamp: " << cb->timestamp << "\n";
                } else if (block->type == MetadataType::EXIF) {
                    const auto* eb = static_cast<const EXIFBlock*>(block);
                    ss << "EXIF\n"
                       << "  Camera Model:  " << eb->camera_model << "\n"
                       << "  Exposure Time: " << eb->exposure_time << "s\n"
                       << "  F-Number:      f/" << eb->f_number << "\n"
                       << "  ISO Speed:     " << eb->iso_speed << "\n";
                }
            }
            return ss.str();
        }
        else if (sub == "view_exif") {
            if (args.size() < 3) return "Error: view_exif requires block index.\n";
            size_t idx = std::stoul(args[2]);
            if (idx >= current_metadata.size()) return "Error: Index out of bounds.\n";
            
            MetadataBlock* block = current_metadata[idx];
            g_last_viewed_block = block;
            // Fast direct cast of metadata block to EXIF structure
            EXIFBlock* exif = get_exif_block(block);
            if (!exif) return "Error: Selected metadata block is not EXIF.\n";

            if (g_deleted_block && g_last_viewed_block == g_deleted_block) {
                volatile auto deleted_type = g_deleted_block->type;
                (void)deleted_type;
            }
            
            std::stringstream ss;
            ss << "=== Type Confused EXIF View ===\n"
               << "Camera Model:  " << exif->camera_model << "\n"
               << "Exposure Time: " << exif->exposure_time << "\n"
               << "F-Number:      " << exif->f_number << "\n"
               << "ISO Speed:     " << exif->iso_speed << "\n"
               << "Thumb Size:    " << exif->thumbnail_size << "\n";
            if (exif->raw_thumbnail && exif->thumbnail_size > 0) {
                ss << "Thumb First:   " << static_cast<int>(exif->raw_thumbnail[0]) << "\n";
            }
            return ss.str();
        }
        
        return "Unknown metadata subcommand: " + sub + "\n";
    }

    return "Unknown command: " + cmd + ". Type 'help' to see active commands.\n";
    } catch (const std::exception& e) {
        return "Error: Invalid argument format or out-of-range value.\n";
    }
}

} // namespace PixelForge
