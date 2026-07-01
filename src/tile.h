#pragma once

#include "image.h"
#include "errors.h"

#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace PixelForge {

// Direction enum for neighbor tile halo data exchange
enum class TileDirection {
    Top = 0,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

// Rectangular region in pixel coordinates
struct TileRegion {
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;

    // True when the region has positive area
    bool is_valid() const { return width > 0 && height > 0; }

    // Pixel count
    int area() const { return width * height; }

    // Right edge (exclusive)
    int right() const { return x + width; }

    // Bottom edge (exclusive)
    int bottom() const { return y + height; }

    // Clamp this region to fit within [0, max_w) x [0, max_h)
    TileRegion clamped(int max_w, int max_h) const {
        TileRegion r;
        r.x      = std::max(x, 0);
        r.y      = std::max(y, 0);
        int rx   = std::min(x + width,  max_w);
        int ry   = std::min(y + height, max_h);
        r.width  = std::max(rx - r.x, 0);
        r.height = std::max(ry - r.y, 0);
        return r;
    }

    // Intersection with another region
    TileRegion intersect(const TileRegion& other) const {
        int ix = std::max(x, other.x);
        int iy = std::max(y, other.y);
        int ir = std::min(right(), other.right());
        int ib = std::min(bottom(), other.bottom());
        TileRegion r;
        r.x      = ix;
        r.y      = iy;
        r.width  = std::max(ir - ix, 0);
        r.height = std::max(ib - iy, 0);
        return r;
    }

    // Check if a point lies inside this region
    bool contains(int px, int py) const {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    // Expand this region by 'amount' on every side
    TileRegion expanded(int amount) const {
        return {x - amount, y - amount, width + 2 * amount, height + 2 * amount};
    }

    bool operator==(const TileRegion& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    bool operator!=(const TileRegion& o) const { return !(*this == o); }
};

// Descriptor for a single tile within a grid decomposition
struct TileDescriptor {
    int        tile_x      = 0;   // grid column index
    int        tile_y      = 0;   // grid row index
    TileRegion region;             // core pixel region
    TileRegion halo_region;        // region including halo border
    int        halo_size   = 0;   // pixels of overlap on each side
};

// A Tile owns pixel data for a rectangular region of an image.
// The internal buffer stores pixels for the *halo_region* (core + overlap).
// Pixel coordinates within the tile are relative to halo_region origin.
class Tile {
public:
    Tile();
    explicit Tile(const TileDescriptor& desc, int channels);
    ~Tile() = default;

    // Non-copyable, movable
    Tile(const Tile&) = delete;
    Tile& operator=(const Tile&) = delete;
    Tile(Tile&& other) noexcept;
    Tile& operator=(Tile&& other) noexcept;

    // Descriptor access
    const TileDescriptor& descriptor() const { return m_desc; }

    // Pixel dimensions of the full allocation (halo region)
    int alloc_width()  const { return m_desc.halo_region.width; }
    int alloc_height() const { return m_desc.halo_region.height; }

    // Channel count
    int channel_count() const { return m_channels; }

    // Dirty flag — set when pixel data has been modified
    bool is_dirty() const { return m_dirty; }
    void mark_dirty()     { m_dirty = true; }
    void clear_dirty()    { m_dirty = false; }

    // Read a pixel at local coordinates (relative to halo_region origin)
    Pixel get_pixel(int local_x, int local_y) const;

    // Write a pixel at local coordinates
    void set_pixel(int local_x, int local_y, const Pixel& p);

    // Raw data access
    const std::vector<uint8_t>& get_data() const { return m_data; }
    std::vector<uint8_t>&       get_data()       { return m_data; }

    // Pointer to the start of a specific row
    const uint8_t* row_ptr(int local_y) const;
    uint8_t*       row_ptr(int local_y);

    // Copy pixel data from an Image into this tile's buffer (core + halo).
    // Reads from the image region described by halo_region, clamping at edges.
    PixelForgeErrorCode copy_from_image(const Image& img);

    // Copy pixel data from an arbitrary region of an image into this tile.
    PixelForgeErrorCode copy_from_image(const Image& img, const TileRegion& src_region);

    // Write the tile's *core* region back into the destination image.
    PixelForgeErrorCode write_to_image(Image& img) const;

    // Write the tile's core region with alpha-blended overlap stitching.
    // 'blend_weight' in [0,1] controls interpolation in the halo border.
    PixelForgeErrorCode write_to_image_blended(Image& img, float blend_weight) const;

    // Return a copy of just the halo border pixels (top/bottom/left/right strips)
    std::vector<uint8_t> get_halo_data(TileDirection dir) const;

    // Apply halo data from a neighbor tile.  Copies the neighbor's edge
    // pixels into this tile's halo border on the given side.
    void apply_halo_from(const Tile& neighbor, TileDirection dir);

    // Fill the entire tile with a constant pixel value.
    void fill(const Pixel& p);

    // Clear all data to zero
    void clear();

    // Memory footprint of the pixel buffer
    size_t memory_size() const { return m_data.size(); }

private:
    TileDescriptor       m_desc;
    std::vector<uint8_t> m_data;
    int                  m_channels = 4;
    bool                 m_dirty    = false;

    // Offset into m_data for a given local pixel
    size_t pixel_offset(int lx, int ly) const;

    // Validate that local coords are in-bounds
    bool in_bounds(int lx, int ly) const;
};

} // namespace PixelForge
