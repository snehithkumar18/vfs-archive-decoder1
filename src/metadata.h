#ifndef PIXELFORGE_METADATA_H
#define PIXELFORGE_METADATA_H

#include <string>
#include <vector>
#include <cstdint>

namespace PixelForge {

enum class MetadataType : uint8_t {
    COMMENTS = 0,
    EXIF = 1
};

struct MetadataBlock {
    MetadataType type;
    virtual ~MetadataBlock() = default;
};

struct CommentsBlock : public MetadataBlock {
    std::string author;
    std::string comment;
    uint32_t timestamp = 0;
};

struct EXIFBlock : public MetadataBlock {
    std::string camera_model;
    float exposure_time = 0.0f;
    float f_number = 0.0f;
    int iso_speed = 0;
    uint8_t* raw_thumbnail = nullptr;
    size_t thumbnail_size = 0;

    EXIFBlock() = default;
    ~EXIFBlock() override;
    
    // Disable copy/assignment to avoid double free of raw_thumbnail easily
    EXIFBlock(const EXIFBlock&) = delete;
    EXIFBlock& operator=(const EXIFBlock&) = delete;
};

// Parsing and serialization functions
std::vector<MetadataBlock*> parse_metadata(const uint8_t* data, size_t size);
std::vector<uint8_t> serialize_metadata(const std::vector<const MetadataBlock*>& blocks);

// Cast helper operations
EXIFBlock* get_exif_block(MetadataBlock* block);
CommentsBlock* get_comments_block(MetadataBlock* block);

} // namespace PixelForge

#endif // PIXELFORGE_METADATA_H
