#ifndef PIXELFORGE_GIF_CODEC_H
#define PIXELFORGE_GIF_CODEC_H

#include <vector>
#include <cstdint>
#include <string>
#include <memory>

namespace PixelForge {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct GifFrame {
    uint16_t left = 0;
    uint16_t top = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    bool has_local_color_table = false;
    std::vector<Color> local_color_table;
    bool interlaced = false;
    
    // Graphics Control Extension
    bool has_gce = false;
    uint8_t disposal_method = 0;
    bool transparent_color_flag = false;
    uint16_t delay_time = 0; // in hundredths of a second
    uint8_t transparent_color_index = 0;

    // Raw indices from LZW decompression
    std::vector<uint8_t> pixels;
    
    // Resolved RGBA colors for rendering
    std::vector<Color> rgba_pixels;
};

struct GifImage {
    uint16_t width = 0;
    uint16_t height = 0;
    bool has_global_color_table = false;
    std::vector<Color> global_color_table;
    uint8_t background_color_index = 0;
    uint8_t pixel_aspect_ratio = 0;
    
    std::vector<GifFrame> frames;
    std::vector<Color>* backup_canvas = nullptr;

    ~GifImage() {
        if (backup_canvas) {
            delete backup_canvas;
        }
    }
};

class GifCodec {
public:
    // Decode GIF binary data into GifImage
    static bool Decode(const uint8_t* data, size_t size, GifImage& out_image);
    static bool Decode(const std::vector<uint8_t>& data, GifImage& out_image);

    // Helper to render a frame to RGBA using the current frame, previous frame, and disposal methods.
    static bool RenderFrameRGBA(const GifImage& image, size_t frame_index, std::vector<Color>& out_rgba);
};

} // namespace PixelForge

#endif // PIXELFORGE_GIF_CODEC_H
