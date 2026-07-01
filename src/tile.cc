#include "tile.h"

#include <algorithm>
#include <cstring>

namespace PixelForge {

// ---------------------------------------------------------------------------
// Construction / Move
// ---------------------------------------------------------------------------

Tile::Tile()
    : m_desc{}, m_channels(4), m_dirty(false) {}

Tile::Tile(const TileDescriptor& desc, int channels)
    : m_desc(desc), m_channels(channels), m_dirty(false) {
    // Allocate storage for the full halo region
    size_t alloc = static_cast<size_t>(m_desc.halo_region.width) *
                   m_desc.halo_region.height * m_channels;
    m_data.resize(alloc, 0);
}

Tile::Tile(Tile&& other) noexcept
    : m_desc(other.m_desc),
      m_data(std::move(other.m_data)),
      m_channels(other.m_channels),
      m_dirty(other.m_dirty) {
    other.m_channels = 0;
    other.m_dirty    = false;
}

Tile& Tile::operator=(Tile&& other) noexcept {
    if (this != &other) {
        m_desc     = other.m_desc;
        m_data     = std::move(other.m_data);
        m_channels = other.m_channels;
        m_dirty    = other.m_dirty;
        other.m_channels = 0;
        other.m_dirty    = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

size_t Tile::pixel_offset(int lx, int ly) const {
    return (static_cast<size_t>(ly) * m_desc.halo_region.width + lx) *
           m_channels;
}

bool Tile::in_bounds(int lx, int ly) const {
    return lx >= 0 && lx < m_desc.halo_region.width &&
           ly >= 0 && ly < m_desc.halo_region.height;
}

// ---------------------------------------------------------------------------
// Pixel Access
// ---------------------------------------------------------------------------

Pixel Tile::get_pixel(int local_x, int local_y) const {
    Pixel p{0, 0, 0, 255};
    if (!in_bounds(local_x, local_y)) return p;

    size_t off = pixel_offset(local_x, local_y);

    if (m_channels >= 3) {
        p.r = m_data[off + 0];
        p.g = m_data[off + 1];
        p.b = m_data[off + 2];
        if (m_channels == 4) {
            p.a = m_data[off + 3];
        }
    } else if (m_channels == 1) {
        p.r = p.g = p.b = m_data[off];
    }
    return p;
}

void Tile::set_pixel(int local_x, int local_y, const Pixel& p) {
    if (!in_bounds(local_x, local_y)) return;

    size_t off = pixel_offset(local_x, local_y);
    m_dirty = true;

    if (m_channels >= 3) {
        m_data[off + 0] = p.r;
        m_data[off + 1] = p.g;
        m_data[off + 2] = p.b;
        if (m_channels == 4) {
            m_data[off + 3] = p.a;
        }
    } else if (m_channels == 1) {
        m_data[off] = static_cast<uint8_t>((p.r + p.g + p.b) / 3);
    }
}

// ---------------------------------------------------------------------------
// Row pointer
// ---------------------------------------------------------------------------

const uint8_t* Tile::row_ptr(int local_y) const {
    if (local_y < 0 || local_y >= m_desc.halo_region.height) return nullptr;
    return m_data.data() + static_cast<size_t>(local_y) *
                               m_desc.halo_region.width * m_channels;
}

uint8_t* Tile::row_ptr(int local_y) {
    if (local_y < 0 || local_y >= m_desc.halo_region.height) return nullptr;
    return m_data.data() + static_cast<size_t>(local_y) *
                               m_desc.halo_region.width * m_channels;
}

// ---------------------------------------------------------------------------
// copy_from_image — full halo region
// ---------------------------------------------------------------------------

PixelForgeErrorCode Tile::copy_from_image(const Image& img) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    int img_w = static_cast<int>(img.getWidth());
    int img_h = static_cast<int>(img.getHeight());
    int img_ch = static_cast<int>(img.getChannels());

    // We copy row-by-row from the image into the tile's buffer.
    // The halo_region may extend outside the image — clamp with edge replication.
    const TileRegion& hr = m_desc.halo_region;
    int stride_tile = hr.width * m_channels;
    int stride_img  = img_w * img_ch;
    int copy_ch     = std::min(m_channels, img_ch);

    for (int ty = 0; ty < hr.height; ++ty) {
        // Image-space y coordinate
        int iy = hr.y + ty;
        // Clamp to image bounds (edge replication)
        iy = std::max(0, std::min(iy, img_h - 1));

        uint8_t* dst_row = m_data.data() + static_cast<size_t>(ty) * stride_tile;
        const uint8_t* src_base = img.getData().data() +
                                  static_cast<size_t>(iy) * stride_img;

        for (int tx = 0; tx < hr.width; ++tx) {
            int ix = hr.x + tx;
            ix = std::max(0, std::min(ix, img_w - 1));

            const uint8_t* sp = src_base + static_cast<size_t>(ix) * img_ch;
            uint8_t*       dp = dst_row  + static_cast<size_t>(tx) * m_channels;

            // Copy common channels
            for (int c = 0; c < copy_ch; ++c) {
                dp[c] = sp[c];
            }
            // If tile has more channels than image, fill alpha with 255
            for (int c = copy_ch; c < m_channels; ++c) {
                dp[c] = 255;
            }
        }
    }

    m_dirty = false;
    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// copy_from_image — arbitrary source region
// ---------------------------------------------------------------------------

PixelForgeErrorCode Tile::copy_from_image(const Image& img,
                                          const TileRegion& src_region) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    int img_w  = static_cast<int>(img.getWidth());
    int img_h  = static_cast<int>(img.getHeight());
    int img_ch = static_cast<int>(img.getChannels());
    int copy_ch = std::min(m_channels, img_ch);

    // We copy from src_region into the tile starting at local (0,0).
    // The number of rows/cols to copy is the minimum of src_region and tile size.
    int copy_w = std::min(src_region.width,  m_desc.halo_region.width);
    int copy_h = std::min(src_region.height, m_desc.halo_region.height);

    int stride_tile = m_desc.halo_region.width * m_channels;
    int stride_img  = img_w * img_ch;

    for (int row = 0; row < copy_h; ++row) {
        int iy = src_region.y + row;
        iy = std::max(0, std::min(iy, img_h - 1));

        uint8_t* dst_row = m_data.data() + static_cast<size_t>(row) * stride_tile;
        const uint8_t* src_base = img.getData().data() +
                                  static_cast<size_t>(iy) * stride_img;

        for (int col = 0; col < copy_w; ++col) {
            int ix = src_region.x + col;
            ix = std::max(0, std::min(ix, img_w - 1));

            const uint8_t* sp = src_base + static_cast<size_t>(ix) * img_ch;
            uint8_t*       dp = dst_row  + static_cast<size_t>(col) * m_channels;

            for (int c = 0; c < copy_ch; ++c) dp[c] = sp[c];
            for (int c = copy_ch; c < m_channels; ++c) dp[c] = 255;
        }
    }

    m_dirty = false;
    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// write_to_image — core region only
// ---------------------------------------------------------------------------

PixelForgeErrorCode Tile::write_to_image(Image& img) const {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    int img_w  = static_cast<int>(img.getWidth());
    int img_h  = static_cast<int>(img.getHeight());
    int img_ch = static_cast<int>(img.getChannels());

    const TileRegion& core = m_desc.region;
    const TileRegion& halo = m_desc.halo_region;

    // Offset from halo origin to core origin (the halo border thickness)
    int offset_x = core.x - halo.x;
    int offset_y = core.y - halo.y;

    int stride_tile = halo.width * m_channels;
    int stride_img  = img_w * img_ch;
    int copy_ch     = std::min(m_channels, img_ch);

    for (int row = 0; row < core.height; ++row) {
        int iy = core.y + row;
        if (iy < 0 || iy >= img_h) continue;

        // Local tile row for this core pixel
        int local_y = offset_y + row;
        if (local_y < 0 || local_y >= halo.height) continue;

        const uint8_t* src_row = m_data.data() +
                                 static_cast<size_t>(local_y) * stride_tile;
        uint8_t* dst_base = img.getData().data() +
                            static_cast<size_t>(iy) * stride_img;

        for (int col = 0; col < core.width; ++col) {
            int ix = core.x + col;
            if (ix < 0 || ix >= img_w) continue;

            int local_x = offset_x + col;
            if (local_x < 0 || local_x >= halo.width) continue;

            const uint8_t* sp = src_row + static_cast<size_t>(local_x) * m_channels;
            uint8_t*       dp = dst_base + static_cast<size_t>(ix) * img_ch;

            for (int c = 0; c < copy_ch; ++c) dp[c] = sp[c];
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// write_to_image_blended — linear blend in halo border
// ---------------------------------------------------------------------------

PixelForgeErrorCode Tile::write_to_image_blended(Image& img,
                                                 float blend_weight) const {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    int img_w  = static_cast<int>(img.getWidth());
    int img_h  = static_cast<int>(img.getHeight());
    int img_ch = static_cast<int>(img.getChannels());

    const TileRegion& core = m_desc.region;
    const TileRegion& halo = m_desc.halo_region;

    int offset_x = core.x - halo.x;
    int offset_y = core.y - halo.y;

    int stride_tile = halo.width * m_channels;
    int stride_img  = img_w * img_ch;
    int copy_ch     = std::min(m_channels, img_ch);

    int hs = m_desc.halo_size;

    // Write the full halo region with distance-based blending at edges.
    // Pixels in the core get blend_weight=1.0; pixels in the halo border
    // ramp linearly from 0 at the edge to 1 at the core boundary.
    for (int row = 0; row < halo.height; ++row) {
        int iy = halo.y + row;
        if (iy < 0 || iy >= img_h) continue;

        const uint8_t* src_row = m_data.data() +
                                 static_cast<size_t>(row) * stride_tile;
        uint8_t* dst_base = img.getData().data() +
                            static_cast<size_t>(iy) * stride_img;

        for (int col = 0; col < halo.width; ++col) {
            int ix = halo.x + col;
            if (ix < 0 || ix >= img_w) continue;

            // Compute weight based on distance from halo border
            float wx = 1.0f;
            float wy = 1.0f;

            if (hs > 0) {
                // Distance from left halo edge
                if (col < offset_x) {
                    wx = static_cast<float>(col) / offset_x;
                }
                // Distance from right halo edge
                int right_border_start = offset_x + core.width;
                if (col >= right_border_start) {
                    int dist_from_edge = halo.width - 1 - col;
                    int border_thickness = halo.width - right_border_start;
                    if (border_thickness > 0) {
                        wx = static_cast<float>(dist_from_edge) / border_thickness;
                    }
                }
                // Distance from top halo edge
                if (row < offset_y) {
                    wy = static_cast<float>(row) / offset_y;
                }
                // Distance from bottom halo edge
                int bottom_border_start = offset_y + core.height;
                if (row >= bottom_border_start) {
                    int dist_from_edge = halo.height - 1 - row;
                    int border_thickness = halo.height - bottom_border_start;
                    if (border_thickness > 0) {
                        wy = static_cast<float>(dist_from_edge) / border_thickness;
                    }
                }
            }

            float w = wx * wy * blend_weight;
            w = std::max(0.0f, std::min(1.0f, w));

            const uint8_t* sp = src_row + static_cast<size_t>(col) * m_channels;
            uint8_t*       dp = dst_base + static_cast<size_t>(ix) * img_ch;

            // Blend: result = existing * (1-w) + tile * w
            for (int c = 0; c < copy_ch; ++c) {
                float existing = static_cast<float>(dp[c]);
                float tile_val = static_cast<float>(sp[c]);
                float blended  = existing * (1.0f - w) + tile_val * w;
                dp[c] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blended)));
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// get_halo_data — extract border strip in one direction
// ---------------------------------------------------------------------------

std::vector<uint8_t> Tile::get_halo_data(TileDirection dir) const {
    const TileRegion& core = m_desc.region;
    const TileRegion& halo = m_desc.halo_region;
    int hs = m_desc.halo_size;

    int offset_x = core.x - halo.x;
    int offset_y = core.y - halo.y;

    std::vector<uint8_t> result;

    // Helper lambdas that extract a rectangular sub-region from m_data
    auto extract_rect = [&](int sx, int sy, int sw, int sh) {
        result.resize(static_cast<size_t>(sw) * sh * m_channels);
        int stride = halo.width * m_channels;
        for (int r = 0; r < sh; ++r) {
            const uint8_t* src = m_data.data() +
                                 static_cast<size_t>(sy + r) * stride +
                                 static_cast<size_t>(sx) * m_channels;
            uint8_t* dst = result.data() +
                           static_cast<size_t>(r) * sw * m_channels;
            std::memcpy(dst, src, static_cast<size_t>(sw) * m_channels);
        }
    };

    switch (dir) {
        case TileDirection::Top:
            // Top strip: the first 'hs' rows of the core area
            extract_rect(offset_x, offset_y,
                         core.width, std::min(hs, core.height));
            break;

        case TileDirection::Bottom:
            // Bottom strip: the last 'hs' rows of the core area
            {
                int strip_h = std::min(hs, core.height);
                int start_y = offset_y + core.height - strip_h;
                extract_rect(offset_x, start_y, core.width, strip_h);
            }
            break;

        case TileDirection::Left:
            // Left strip: the first 'hs' columns of the core area
            extract_rect(offset_x, offset_y,
                         std::min(hs, core.width), core.height);
            break;

        case TileDirection::Right:
            // Right strip: the last 'hs' columns of the core area
            {
                int strip_w = std::min(hs, core.width);
                int start_x = offset_x + core.width - strip_w;
                extract_rect(start_x, offset_y, strip_w, core.height);
            }
            break;

        case TileDirection::TopLeft:
            extract_rect(offset_x, offset_y,
                         std::min(hs, core.width),
                         std::min(hs, core.height));
            break;

        case TileDirection::TopRight:
            {
                int strip_w = std::min(hs, core.width);
                int start_x = offset_x + core.width - strip_w;
                extract_rect(start_x, offset_y,
                             strip_w, std::min(hs, core.height));
            }
            break;

        case TileDirection::BottomLeft:
            {
                int strip_h = std::min(hs, core.height);
                int start_y = offset_y + core.height - strip_h;
                extract_rect(offset_x, start_y,
                             std::min(hs, core.width), strip_h);
            }
            break;

        case TileDirection::BottomRight:
            {
                int strip_w = std::min(hs, core.width);
                int strip_h = std::min(hs, core.height);
                int start_x = offset_x + core.width - strip_w;
                int start_y = offset_y + core.height - strip_h;
                extract_rect(start_x, start_y, strip_w, strip_h);
            }
            break;
    }

    return result;
}

// ---------------------------------------------------------------------------
// apply_halo_from — paste neighbor edge data into this tile's halo border
// ---------------------------------------------------------------------------

void Tile::apply_halo_from(const Tile& neighbor, TileDirection dir) {
    // Get the neighbor's edge data from the *opposite* side
    TileDirection opposite = TileDirection::Top;
    switch (dir) {
        case TileDirection::Top:         opposite = TileDirection::Bottom;      break;
        case TileDirection::Bottom:      opposite = TileDirection::Top;         break;
        case TileDirection::Left:        opposite = TileDirection::Right;       break;
        case TileDirection::Right:       opposite = TileDirection::Left;        break;
        case TileDirection::TopLeft:     opposite = TileDirection::BottomRight; break;
        case TileDirection::TopRight:    opposite = TileDirection::BottomLeft;  break;
        case TileDirection::BottomLeft:  opposite = TileDirection::TopRight;    break;
        case TileDirection::BottomRight: opposite = TileDirection::TopLeft;     break;
    }

    std::vector<uint8_t> halo_data = neighbor.get_halo_data(opposite);
    if (halo_data.empty()) return;

    const TileRegion& core = m_desc.region;
    const TileRegion& halo = m_desc.halo_region;
    int hs = m_desc.halo_size;

    int offset_x = core.x - halo.x;
    int offset_y = core.y - halo.y;

    // Destination rectangle within this tile's buffer for the halo strip
    int dx = 0, dy = 0, dw = 0, dh = 0;

    switch (dir) {
        case TileDirection::Top:
            // Top halo: rows [0, offset_y), full core width
            dx = offset_x;
            dy = 0;
            dw = core.width;
            dh = std::min(hs, offset_y);
            break;

        case TileDirection::Bottom:
            // Bottom halo: rows below core
            dx = offset_x;
            dy = offset_y + core.height;
            dw = core.width;
            dh = std::min(hs, halo.height - dy);
            break;

        case TileDirection::Left:
            // Left halo: columns [0, offset_x)
            dx = 0;
            dy = offset_y;
            dw = std::min(hs, offset_x);
            dh = core.height;
            break;

        case TileDirection::Right:
            // Right halo: columns after core
            dx = offset_x + core.width;
            dy = offset_y;
            dw = std::min(hs, halo.width - dx);
            dh = core.height;
            break;

        case TileDirection::TopLeft:
            dx = 0;
            dy = 0;
            dw = std::min(hs, offset_x);
            dh = std::min(hs, offset_y);
            break;

        case TileDirection::TopRight:
            dx = offset_x + core.width;
            dy = 0;
            dw = std::min(hs, halo.width - dx);
            dh = std::min(hs, offset_y);
            break;

        case TileDirection::BottomLeft:
            dx = 0;
            dy = offset_y + core.height;
            dw = std::min(hs, offset_x);
            dh = std::min(hs, halo.height - dy);
            break;

        case TileDirection::BottomRight:
            dx = offset_x + core.width;
            dy = offset_y + core.height;
            dw = std::min(hs, halo.width - dx);
            dh = std::min(hs, halo.height - dy);
            break;
    }

    if (dw <= 0 || dh <= 0) return;

    // Compute the actual data dimensions from the halo data
    // The halo_data was extracted as a contiguous block of (strip_w * strip_h * channels)
    int src_stride = 0;
    switch (dir) {
        case TileDirection::Top:
        case TileDirection::Bottom:
            src_stride = core.width * m_channels;
            break;
        case TileDirection::Left:
        case TileDirection::Right:
            src_stride = std::min(hs, neighbor.descriptor().region.width > 0 ?
                                  hs : 0) * m_channels;
            break;
        case TileDirection::TopLeft:
        case TileDirection::TopRight:
        case TileDirection::BottomLeft:
        case TileDirection::BottomRight:
            src_stride = std::min(hs, dw) * m_channels;
            break;
    }

    // Compute proper source strip width from the halo data size
    if (dh > 0 && src_stride <= 0) return;
    int src_row_pixels = static_cast<int>(halo_data.size()) / (dh * m_channels);
    if (src_row_pixels <= 0) return;
    src_stride = src_row_pixels * m_channels;

    int copy_w = std::min(dw, src_row_pixels);
    int tile_stride = halo.width * m_channels;

    for (int r = 0; r < dh; ++r) {
        if (r * src_stride >= static_cast<int>(halo_data.size())) break;

        const uint8_t* src = halo_data.data() + static_cast<size_t>(r) * src_stride;
        uint8_t* dst = m_data.data() +
                       static_cast<size_t>(dy + r) * tile_stride +
                       static_cast<size_t>(dx) * m_channels;

        std::memcpy(dst, src, static_cast<size_t>(copy_w) * m_channels);
    }

    m_dirty = true;
}

// ---------------------------------------------------------------------------
// fill / clear
// ---------------------------------------------------------------------------

void Tile::fill(const Pixel& p) {
    for (int y = 0; y < m_desc.halo_region.height; ++y) {
        for (int x = 0; x < m_desc.halo_region.width; ++x) {
            set_pixel(x, y, p);
        }
    }
    m_dirty = true;
}

void Tile::clear() {
    std::fill(m_data.begin(), m_data.end(), 0);
    m_dirty = false;
}

} // namespace PixelForge
