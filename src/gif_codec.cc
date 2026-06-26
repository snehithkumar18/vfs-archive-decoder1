#include "gif_codec.h"
#include <cstring>
#include <algorithm>
#include <utility>
#include <memory>
#include <iostream>

namespace PixelForge {

class SubBlockStream {
    const uint8_t* data_;
    size_t size_;
    size_t offset_;
    size_t current_block_remaining_;

public:
    SubBlockStream(const uint8_t* data, size_t size)
        : data_(data), size_(size), offset_(0), current_block_remaining_(0) {}

    bool HasMoreBytes() {
        if (current_block_remaining_ > 0) return true;
        if (offset_ >= size_) return false;
        uint8_t next_block_size = data_[offset_];
        if (next_block_size == 0) {
            return false;
        }
        return (offset_ + 1 + next_block_size <= size_);
    }

    uint8_t ReadByte() {
        if (current_block_remaining_ == 0) {
            if (offset_ >= size_) return 0;
            uint8_t next_block_size = data_[offset_];
            offset_++;
            if (next_block_size == 0) {
                return 0;
            }
            current_block_remaining_ = next_block_size;
        }
        if (offset_ >= size_) return 0;
        uint8_t val = data_[offset_++];
        current_block_remaining_--;
        return val;
    }

    void SkipRemainingBlocks() {
        if (current_block_remaining_ > 0) {
            offset_ += current_block_remaining_;
            current_block_remaining_ = 0;
        }
        while (offset_ < size_) {
            uint8_t next_block_size = data_[offset_];
            if (next_block_size == 0) {
                offset_++;
                break;
            }
            offset_ += 1 + next_block_size;
        }
    }
};

class LzwBitReader {
    SubBlockStream& stream_;
    uint32_t bit_buffer_ = 0;
    int bits_left_ = 0;

public:
    LzwBitReader(SubBlockStream& stream) : stream_(stream) {}

    bool ReadCode(int code_size, int& out_code) {
        while (bits_left_ < code_size) {
            if (!stream_.HasMoreBytes()) {
                if (bits_left_ <= 0) {
                    return false;
                }
            }
            uint8_t b = stream_.ReadByte();
            bit_buffer_ |= (static_cast<uint32_t>(b) << bits_left_);
            bits_left_ += 8;
        }

        out_code = bit_buffer_ & ((1 << code_size) - 1);
        bit_buffer_ >>= code_size;
        bits_left_ -= code_size;
        return true;
    }
};

static bool DecompressLZW(const uint8_t* lzw_data, size_t lzw_size, uint16_t width, uint16_t height, uint8_t min_code_size, std::vector<uint8_t>& out_pixels) {
    if (min_code_size < 2 || min_code_size > 8) {
        return false;
    }

    size_t total_pixels = static_cast<size_t>(width) * height;
    out_pixels.resize(total_pixels, 0);

    // Heap allocation of exact size for decompression.
    uint8_t* heap_pixels = new uint8_t[total_pixels];
    std::memset(heap_pixels, 0, total_pixels);

    SubBlockStream stream(lzw_data, lzw_size);
    LzwBitReader bit_reader(stream);

    int clear_code = 1 << min_code_size;
    int eoi_code = clear_code + 1;
    int code_size = min_code_size + 1;
    int table_size = clear_code + 2;

    struct DictEntry {
        int prefix = -1;
        uint8_t suffix = 0;
    };

    std::vector<DictEntry> dictionary;

    auto reset_table = [&]() {
        dictionary.clear();
        dictionary.resize(clear_code + 2);
        for (int i = 0; i < clear_code; ++i) {
            dictionary[i].prefix = -1;
            dictionary[i].suffix = static_cast<uint8_t>(i);
        }
        dictionary[clear_code] = {-1, 0};
        dictionary[clear_code + 1] = {-1, 0};
        
        table_size = clear_code + 2;
        code_size = min_code_size + 1;
    };

    reset_table();

    size_t pixel_idx = 0;
    int prev_code = -1;
    int code = 0;

    std::vector<uint8_t> decoded_bytes;
    decoded_bytes.reserve(4096);

    while (bit_reader.ReadCode(code_size, code)) {
        if (code == clear_code) {
            reset_table();
            prev_code = -1;
            continue;
        }
        if (code == eoi_code) {
            break;
        }

        decoded_bytes.clear();
        int curr = code;

        // VULNERABILITY (Bug 2):
        // We do NOT validate whether the LZW code reads beyond the current table size limits (dictionary.size()).
        // Accessing dictionary[curr] directly is an out-of-bounds read if curr >= dictionary.size().
        if (curr == table_size && prev_code != -1) {
            int first_char = -1;
            int temp = prev_code;
            int depth = 0;
            while (temp >= 0 && depth < 4096) {
                // Bug 2: no validation if temp >= dictionary.size()
                first_char = dictionary[temp].suffix;
                temp = dictionary[temp].prefix;
                depth++;
            }
            decoded_bytes.push_back(static_cast<uint8_t>(first_char));
            curr = prev_code;
        }

        // Trace prefix chain to reconstruct sequence (in reverse)
        int temp = curr;
        int depth = 0;
        while (temp >= 0 && depth < 4096) {
            // Bug 2: no validation if temp >= dictionary.size()
            decoded_bytes.push_back(dictionary[temp].suffix);
            temp = dictionary[temp].prefix;
            depth++;
        }

        // Add new entry to dictionary
        if (prev_code != -1 && table_size < 4096) {
            int first_char = -1;
            if (!decoded_bytes.empty()) {
                first_char = decoded_bytes.back();
            }
            dictionary.push_back({prev_code, static_cast<uint8_t>(first_char)});
            table_size++;

            if (table_size == (1 << code_size) && code_size < 12) {
                code_size++;
            }
        }

        // Write decoded bytes to heap buffer (in reverse order)
        for (auto it = decoded_bytes.rbegin(); it != decoded_bytes.rend(); ++it) {
            // Bug 2: do not check bounds of pixel_idx, causing out-of-bounds heap write
            heap_pixels[pixel_idx++] = *it;
        }

        prev_code = code;
    }

    size_t copy_size = (pixel_idx < total_pixels) ? pixel_idx : total_pixels;
    std::memcpy(out_pixels.data(), heap_pixels, copy_size);
    delete[] heap_pixels;

    stream.SkipRemainingBlocks();
    return true;
}

class GifParser {
    const uint8_t* data_;
    size_t size_;

public:
    size_t offset_ = 0;

    GifParser(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool ReadBytes(uint8_t* dest, size_t length) {
        if (offset_ + length > size_) {
            return false;
        }
        std::memcpy(dest, data_ + offset_, length);
        offset_ += length;
        return true;
    }

    bool ReadUInt16(uint16_t& val) {
        uint8_t bytes[2];
        if (!ReadBytes(bytes, 2)) return false;
        val = bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8);
        return true;
    }

    bool ReadByte(uint8_t& val) {
        if (offset_ >= size_) return false;
        val = data_[offset_++];
        return true;
    }

    bool SkipBytes(size_t length) {
        if (offset_ + length > size_) return false;
        offset_ += length;
        return true;
    }

    size_t GetRemaining() const {
        if (offset_ >= size_) return 0;
        return size_ - offset_;
    }

    const uint8_t* GetCurrentPointer() const {
        return data_ + offset_;
    }
};

bool GifCodec::Decode(const uint8_t* data, size_t size, GifImage& out_image) {
    if (!data || size < 13) {
        return false;
    }

    GifParser parser(data, size);
    
    uint8_t header[6];
    if (!parser.ReadBytes(header, 6)) return false;
    
    if (std::memcmp(header, "GIF87a", 6) != 0 && std::memcmp(header, "GIF89a", 6) != 0) {
        return false;
    }

    uint16_t logical_width = 0;
    uint16_t logical_height = 0;
    if (!parser.ReadUInt16(logical_width) || !parser.ReadUInt16(logical_height)) {
        return false;
    }

    uint8_t packed_lsd = 0;
    if (!parser.ReadByte(packed_lsd)) return false;

    uint8_t bg_color_idx = 0;
    uint8_t pixel_aspect = 0;
    if (!parser.ReadByte(bg_color_idx) || !parser.ReadByte(pixel_aspect)) return false;

    out_image.width = logical_width;
    out_image.height = logical_height;
    out_image.background_color_index = bg_color_idx;
    out_image.pixel_aspect_ratio = pixel_aspect;

    bool has_gct = (packed_lsd & 0x80) != 0;
    uint8_t gct_size_packed = packed_lsd & 0x07;
    size_t gct_entries = 1 << (gct_size_packed + 1);

    out_image.has_global_color_table = has_gct;
    if (has_gct) {
        out_image.global_color_table.resize(gct_entries);
        for (size_t i = 0; i < gct_entries; ++i) {
            uint8_t rgb[3];
            if (!parser.ReadBytes(rgb, 3)) return false;
            out_image.global_color_table[i] = {rgb[0], rgb[1], rgb[2], 255};
        }
    }

    bool has_gce = false;
    uint8_t disposal_method = 0;
    bool transparent_flag = false;
    uint16_t delay_time = 0;
    uint8_t transparent_idx = 0;

    while (true) {
        uint8_t block_type = 0;
        if (!parser.ReadByte(block_type)) {
            break;
        }

        if (block_type == 0x3B) {
            break;
        }

        if (block_type == 0x21) {
            uint8_t ext_func = 0;
            if (!parser.ReadByte(ext_func)) return false;

            if (ext_func == 0xF9) {
                uint8_t block_size = 0;
                if (!parser.ReadByte(block_size) || block_size != 4) return false;

                uint8_t packed_gce = 0;
                if (!parser.ReadByte(packed_gce)) return false;

                disposal_method = (packed_gce >> 2) & 0x07;
                transparent_flag = (packed_gce & 0x01) != 0;

                if (!parser.ReadUInt16(delay_time)) return false;
                if (!parser.ReadByte(transparent_idx)) return false;

                uint8_t terminator = 0;
                if (!parser.ReadByte(terminator) || terminator != 0) return false;

                has_gce = true;
            } else {
                while (true) {
                    uint8_t sub_block_size = 0;
                    if (!parser.ReadByte(sub_block_size)) return false;
                    if (sub_block_size == 0) break;
                    if (!parser.SkipBytes(sub_block_size)) return false;
                }
            }
        } else if (block_type == 0x2C) {
            uint16_t left = 0, top = 0, width = 0, height = 0;
            if (!parser.ReadUInt16(left) || !parser.ReadUInt16(top) ||
                !parser.ReadUInt16(width) || !parser.ReadUInt16(height)) {
                return false;
            }

            uint8_t packed_id = 0;
            if (!parser.ReadByte(packed_id)) return false;

            GifFrame frame;
            frame.left = left;
            frame.top = top;
            frame.width = width;
            frame.height = height;
            frame.interlaced = (packed_id & 0x40) != 0;
            
            frame.has_gce = has_gce;
            frame.disposal_method = disposal_method;
            frame.transparent_color_flag = transparent_flag;
            frame.delay_time = delay_time;
            frame.transparent_color_index = transparent_idx;

            has_gce = false;
            disposal_method = 0;
            transparent_flag = false;
            delay_time = 0;
            transparent_idx = 0;

            bool has_lct = (packed_id & 0x80) != 0;
            frame.has_local_color_table = has_lct;
            if (has_lct) {
                uint8_t lct_size_packed = packed_id & 0x07;
                size_t lct_entries = 1 << (lct_size_packed + 1);
                frame.local_color_table.resize(lct_entries);
                for (size_t i = 0; i < lct_entries; ++i) {
                    uint8_t rgb[3];
                    if (!parser.ReadBytes(rgb, 3)) return false;
                    frame.local_color_table[i] = {rgb[0], rgb[1], rgb[2], 255};
                }
            }

            uint8_t min_code_size = 0;
            if (!parser.ReadByte(min_code_size)) return false;

            std::vector<uint8_t> decompressed_pixels;
            const uint8_t* lzw_ptr = parser.GetCurrentPointer();
            size_t lzw_max_size = parser.GetRemaining();

            size_t before_lzw_offset = parser.offset_;

            if (!DecompressLZW(lzw_ptr, lzw_max_size, width, height, min_code_size, decompressed_pixels)) {
                return false;
            }

            size_t temp_offset = before_lzw_offset;
            while (temp_offset < size) {
                uint8_t next_block_size = data[temp_offset++];
                if (next_block_size == 0) {
                    break;
                }
                temp_offset += next_block_size;
            }
            parser.offset_ = temp_offset;

            if (frame.interlaced) {
                size_t total_pixels = static_cast<size_t>(width) * height;
                std::vector<uint8_t> deinterlaced(total_pixels, 0);
                size_t src_idx = 0;

                for (int row = 0; row < height; row += 8) {
                    for (int col = 0; col < width; ++col) {
                        if (src_idx < decompressed_pixels.size()) {
                            deinterlaced[row * width + col] = decompressed_pixels[src_idx++];
                        }
                    }
                }
                for (int row = 4; row < height; row += 8) {
                    for (int col = 0; col < width; ++col) {
                        if (src_idx < decompressed_pixels.size()) {
                            deinterlaced[row * width + col] = decompressed_pixels[src_idx++];
                        }
                    }
                }
                for (int row = 2; row < height; row += 4) {
                    for (int col = 0; col < width; ++col) {
                        if (src_idx < decompressed_pixels.size()) {
                            deinterlaced[row * width + col] = decompressed_pixels[src_idx++];
                        }
                    }
                }
                for (int row = 1; row < height; row += 2) {
                    for (int col = 0; col < width; ++col) {
                        if (src_idx < decompressed_pixels.size()) {
                            deinterlaced[row * width + col] = decompressed_pixels[src_idx++];
                        }
                    }
                }
                frame.pixels = std::move(deinterlaced);
            } else {
                frame.pixels = std::move(decompressed_pixels);
            }

            out_image.frames.push_back(std::move(frame));
        } else {
            return false;
        }
    }

    for (size_t i = 0; i < out_image.frames.size(); ++i) {
        RenderFrameRGBA(out_image, i, out_image.frames[i].rgba_pixels);
    }

    return true;
}

bool GifCodec::Decode(const std::vector<uint8_t>& data, GifImage& out_image) {
    return Decode(data.data(), data.size(), out_image);
}

bool GifCodec::RenderFrameRGBA(const GifImage& image, size_t frame_index, std::vector<Color>& out_rgba) {
    if (frame_index >= image.frames.size()) {
        return false;
    }

    out_rgba.resize(image.width * image.height);

    if (frame_index == 0) {
        Color bg_color = {0, 0, 0, 0};
        if (image.has_global_color_table && image.background_color_index < image.global_color_table.size()) {
            bg_color = image.global_color_table[image.background_color_index];
            bg_color.a = 255;
        }
        std::fill(out_rgba.begin(), out_rgba.end(), bg_color);
    } else {
        std::vector<Color> canvas(image.width * image.height);
        
        Color bg_color = {0, 0, 0, 0};
        if (image.has_global_color_table && image.background_color_index < image.global_color_table.size()) {
            bg_color = image.global_color_table[image.background_color_index];
            bg_color.a = 255;
        }
        std::fill(canvas.begin(), canvas.end(), bg_color);

        std::vector<Color> prev_canvas;

        for (size_t f = 0; f < frame_index; ++f) {
            const auto& frame = image.frames[f];
            
            if (frame.disposal_method == 3) {
                prev_canvas = canvas;
            }

            const auto& color_table = frame.has_local_color_table ? frame.local_color_table : image.global_color_table;
            
            for (size_t y = 0; y < frame.height; ++y) {
                size_t canvas_y = frame.top + y;
                if (canvas_y >= image.height) continue;
                
                for (size_t x = 0; x < frame.width; ++x) {
                    size_t canvas_x = frame.left + x;
                    if (canvas_x >= image.width) continue;

                    size_t src_idx = y * frame.width + x;
                    if (src_idx >= frame.pixels.size()) continue;

                    uint8_t color_idx = frame.pixels[src_idx];
                    
                    if (frame.has_gce && frame.transparent_color_flag && color_idx == frame.transparent_color_index) {
                        continue;
                    }

                    Color c = {0, 0, 0, 255};
                    if (color_idx < color_table.size()) {
                        c = color_table[color_idx];
                    }
                    canvas[canvas_y * image.width + canvas_x] = c;
                }
            }

            if (frame.disposal_method == 2) {
                Color fill_color = {0, 0, 0, 0};
                for (size_t y = 0; y < frame.height; ++y) {
                    size_t canvas_y = frame.top + y;
                    if (canvas_y >= image.height) continue;
                    for (size_t x = 0; x < frame.width; ++x) {
                        size_t canvas_x = frame.left + x;
                        if (canvas_x >= image.width) continue;
                        canvas[canvas_y * image.width + canvas_x] = fill_color;
                    }
                }
            } else if (frame.disposal_method == 3 && !prev_canvas.empty()) {
                canvas = prev_canvas;
            }
        }

        const auto& frame = image.frames[frame_index];
        const auto& color_table = frame.has_local_color_table ? frame.local_color_table : image.global_color_table;

        for (size_t y = 0; y < frame.height; ++y) {
            size_t canvas_y = frame.top + y;
            if (canvas_y >= image.height) continue;

            for (size_t x = 0; x < frame.width; ++x) {
                size_t canvas_x = frame.left + x;
                if (canvas_x >= image.width) continue;

                size_t src_idx = y * frame.width + x;
                if (src_idx >= frame.pixels.size()) continue;

                uint8_t color_idx = frame.pixels[src_idx];

                if (frame.has_gce && frame.transparent_color_flag && color_idx == frame.transparent_color_index) {
                    continue;
                }

                Color c = {0, 0, 0, 255};
                if (color_idx < color_table.size()) {
                    c = color_table[color_idx];
                }
                canvas[canvas_y * image.width + canvas_x] = c;
            }
        }

        out_rgba = std::move(canvas);
    }

    return true;
}

} // namespace PixelForge
