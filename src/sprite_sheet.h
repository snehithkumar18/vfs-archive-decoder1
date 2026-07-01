#pragma once

#include "image.h"
#include "animation.h"
#include "errors.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace PixelForge {

// Metadata for a single frame within a sprite sheet
struct SpriteFrameInfo {
    int x;          // X position in sheet
    int y;          // Y position in sheet
    int width;      // Frame width
    int height;     // Frame height
    int index;      // Original frame index
};

// Options for sprite sheet generation
struct SpriteSheetOptions {
    int columns        = 0;     // 0 = auto-compute optimal column count
    int padding        = 0;     // Pixels of padding between frames
    bool power_of_two  = false; // Round sheet dimensions up to power of two
    Pixel pad_color    = {0, 0, 0, 0}; // Transparent padding
};

class SpriteSheet {
public:
    // -----------------------------------------------------------------------
    // Generation — create a sprite sheet from animation frames
    // -----------------------------------------------------------------------

    // Generate a sprite sheet image from an animation.
    // Returns a newly allocated Image.  Caller owns it.
    // Also populates `out_metadata` with per-frame positions if non-null.
    static Image* generate(const Animation& anim,
                           const SpriteSheetOptions& opts = {},
                           std::vector<SpriteFrameInfo>* out_metadata = nullptr);

    // Overload with simple parameters
    static Image* generate(const Animation& anim,
                           int cols, int padding = 0);

    // -----------------------------------------------------------------------
    // Extraction — extract animation frames from a sprite sheet
    // -----------------------------------------------------------------------

    // Extract frames from a sprite sheet image given uniform frame dimensions.
    // Returns a new Animation.  `count` is the number of frames to extract
    // (0 = extract all that fit).
    static Animation extract(const Image& sheet,
                             int frame_w, int frame_h,
                             int count = 0,
                             int delay_ms = 100);

    // Extract frames using explicit metadata positions
    static Animation extract(const Image& sheet,
                             const std::vector<SpriteFrameInfo>& metadata,
                             int delay_ms = 100);

    // -----------------------------------------------------------------------
    // Layout utilities
    // -----------------------------------------------------------------------

    // Compute optimal number of columns for a given frame count
    // (aims for roughly square sheet)
    static int compute_optimal_columns(int frame_count,
                                        int frame_w, int frame_h);

    // Round up to the next power of two
    static int next_power_of_two(int value);

    // Compute the sheet dimensions for given parameters
    static void compute_sheet_size(int frame_count, int frame_w, int frame_h,
                                    int columns, int padding, bool pot,
                                    int& out_width, int& out_height);
};

} // namespace PixelForge
