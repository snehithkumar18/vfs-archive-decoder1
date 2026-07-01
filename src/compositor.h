#pragma once

#include "image.h"
#include "layer.h"
#include "blend.h"
#include "errors.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <functional>

namespace PixelForge {

// Options controlling compositor behaviour
struct CompositorOptions {
    bool cache_enabled       = true;
    bool clear_before_render = true;       // Fill canvas with background before compositing
    Pixel background_color   = {0, 0, 0, 0}; // Transparent black by default
    bool force_opaque_output = false;      // Set alpha=255 on final output
    int  max_cache_entries   = 256;
};

// Statistics from the last composite call
struct CompositorStats {
    int layers_rendered  = 0;
    int layers_skipped   = 0;
    int cache_hits       = 0;
    int cache_misses     = 0;
    size_t peak_memory   = 0;   // bytes of temp images allocated
};

class Compositor {
public:
    // Create a compositor for the given canvas dimensions
    Compositor(uint32_t canvas_width, uint32_t canvas_height);
    ~Compositor();

    // Non-copyable
    Compositor(const Compositor&) = delete;
    Compositor& operator=(const Compositor&) = delete;

    // -----------------------------------------------------------------------
    // Full compositing
    // -----------------------------------------------------------------------

    // Composite the entire layer tree rooted at `root` and return a pointer
    // to the internal result image.  The pointer is valid until the next call
    // to composite() or the Compositor is destroyed.
    Image* composite(Layer* root);

    // Composite only the specified rectangular region of the canvas.
    // Returns a newly allocated Image of size (w x h).  Caller owns it.
    Image* composite_region(Layer* root, int x, int y, int w, int h);

    // -----------------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------------

    // Invalidate cached render for a specific layer and its ancestors
    void invalidate(Layer* layer);

    // Invalidate everything
    void invalidate_all();

    // Enable or disable caching
    void set_cache_enabled(bool enabled);
    bool is_cache_enabled() const { return m_options.cache_enabled; }

    // -----------------------------------------------------------------------
    // Options and stats
    // -----------------------------------------------------------------------

    void set_options(const CompositorOptions& opts);
    const CompositorOptions& options() const { return m_options; }

    void set_background(const Pixel& color);

    // Canvas resize
    void resize_canvas(uint32_t w, uint32_t h);
    uint32_t canvas_width() const { return m_canvas_width; }
    uint32_t canvas_height() const { return m_canvas_height; }

    // Statistics from the last render
    const CompositorStats& stats() const { return m_stats; }

private:
    // -----------------------------------------------------------------------
    // Internal compositing pipeline
    // -----------------------------------------------------------------------

    // Composite a single layer onto the canvas.  Called recursively for groups.
    void composite_layer(Layer* layer, Image* canvas);

    // Composite a group layer: render children to temp buffer, then blend
    void composite_group(Layer* layer, Image* canvas);

    // Composite a pixel layer
    void composite_pixel_layer(Layer* layer, Image* canvas);

    // Composite an adjustment layer (modifies canvas in-place)
    void composite_adjustment(Layer* layer, Image* canvas);

    // Apply a mask image to a rendered result (modulates alpha)
    void apply_mask(Image* result, const Image* mask, int ox, int oy);

    // Apply adjustment parameters to a region of the canvas
    void apply_adjustment_to_region(Image* canvas, const AdjustmentParams& adj,
                                     int x, int y, int w, int h);

    // Fill an image region with the background colour
    void fill_background(Image* canvas, int x, int y, int w, int h);
    void fill_background(Image* canvas);

    // Allocate a temporary canvas-sized RGBA image
    std::unique_ptr<Image> alloc_temp_canvas();
    std::unique_ptr<Image> alloc_temp_canvas(int w, int h);

    // -----------------------------------------------------------------------
    // Render cache
    // -----------------------------------------------------------------------

    // Try to retrieve a cached composite for `layer`.
    // Returns nullptr on miss.
    Image* cache_get(Layer* layer);

    // Store a composite result in cache.  Makes a deep copy.
    void cache_put(Layer* layer, const Image* img);

    // Evict entries over the limit
    void cache_evict();

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    uint32_t m_canvas_width;
    uint32_t m_canvas_height;

    std::unique_ptr<Image> m_result;   // The final composited image

    // Render cache: maps layer pointer -> owned cached image
    std::unordered_map<Layer*, std::unique_ptr<Image>> m_cache;
    std::vector<Layer*> m_cache_order;  // LRU order (most recent at back)

    CompositorOptions m_options;
    CompositorStats   m_stats;
};

} // namespace PixelForge
