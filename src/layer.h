#pragma once

#include "image.h"
#include "blend.h"
#include "errors.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

namespace PixelForge {

// The kind of content a layer holds
enum class LayerType {
    Pixel,       // Raster image content
    Group,       // Container that composites its children
    Adjustment,  // Non-destructive adjustment (brightness, contrast, etc.)
    Mask         // Luminance mask applied to parent layer
};

// Rectangle used for layer bounds
struct LayerRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool empty() const { return w <= 0 || h <= 0; }
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    // Intersection of two rects
    LayerRect intersect(const LayerRect& o) const {
        int ix = std::max(x, o.x);
        int iy = std::max(y, o.y);
        int ix2 = std::min(x + w, o.x + o.w);
        int iy2 = std::min(y + h, o.y + o.h);
        if (ix2 <= ix || iy2 <= iy) return {0, 0, 0, 0};
        return {ix, iy, ix2 - ix, iy2 - iy};
    }
    // Union (bounding box) of two rects
    LayerRect unite(const LayerRect& o) const {
        if (empty()) return o;
        if (o.empty()) return *this;
        int ux = std::min(x, o.x);
        int uy = std::min(y, o.y);
        int ux2 = std::max(x + w, o.x + o.w);
        int uy2 = std::max(y + h, o.y + o.h);
        return {ux, uy, ux2 - ux, uy2 - uy};
    }
};

// Adjustment parameters for adjustment layers
struct AdjustmentParams {
    float brightness = 0.0f;     // [-1..1]
    float contrast   = 0.0f;     // [-1..1]
    float saturation = 0.0f;     // [-1..1]
    float hue_shift  = 0.0f;     // degrees [-180..180]
    float gamma      = 1.0f;     // > 0
    float exposure   = 0.0f;     // EV stops [-5..5]
};

class Layer {
public:
    // Construct a layer of the given type with a name
    explicit Layer(const std::string& name = "Layer",
                   LayerType type = LayerType::Pixel);
    ~Layer() = default;

    // Layers are non-copyable by default (use duplicate() for deep copy)
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    // Move is allowed
    Layer(Layer&&) noexcept = default;
    Layer& operator=(Layer&&) noexcept = default;

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------
    std::string   name;
    LayerType     type        = LayerType::Pixel;
    BlendMode     blend_mode  = BlendMode::Normal;
    float         opacity     = 1.0f;
    bool          visible     = true;
    bool          locked      = false;
    int           position_x  = 0;
    int           position_y  = 0;
    bool          isolated    = true;   // For group layers: isolate blending
    bool          pass_through = false; // For group layers: pass-through mode

    // Pixel content (owned).  nullptr for Group layers.
    std::unique_ptr<Image> content;

    // Optional mask (owned).  Applied as alpha multiplier during compositing.
    std::unique_ptr<Image> mask;

    // Adjustment parameters (only meaningful for Adjustment layers)
    AdjustmentParams adjustment;

    // -----------------------------------------------------------------------
    // Child management (for Group layers)
    // -----------------------------------------------------------------------

    // Number of direct children
    size_t child_count() const { return m_children.size(); }

    // Access child by index (bounds-checked)
    Layer* child_at(size_t index) const;

    // Access all children (read-only iteration)
    const std::vector<std::unique_ptr<Layer>>& children() const {
        return m_children;
    }

    // Add a child at the end (top of stack).  Returns the raw pointer.
    Layer* add_child(std::unique_ptr<Layer> child);

    // Insert a child at a specific index.  Clamps to valid range.
    Layer* insert_child(size_t index, std::unique_ptr<Layer> child);

    // Remove and return the child at index.  Returns nullptr if invalid.
    std::unique_ptr<Layer> remove_child(size_t index);

    // Move a child from position `from` to position `to`.
    PixelForgeErrorCode move_child(size_t from, size_t to);

    // -----------------------------------------------------------------------
    // Tree navigation
    // -----------------------------------------------------------------------
    Layer* parent = nullptr;   // Non-owning back pointer (maintained by add/remove)

    // Depth in the layer tree (root = 0)
    int depth() const;

    // Walk up to root
    Layer* root();
    const Layer* root() const;

    // Index of this layer within its parent's children, or -1
    int index_in_parent() const;

    // -----------------------------------------------------------------------
    // Search
    // -----------------------------------------------------------------------

    // Find a descendant by name (depth-first).  Returns nullptr if not found.
    Layer* find_by_name(const std::string& search_name);
    const Layer* find_by_name(const std::string& search_name) const;

    // Collect all descendants into a flat list (pre-order traversal)
    void collect_all(std::vector<Layer*>& out);

    // Walk all descendants calling a visitor function
    void walk(const std::function<void(Layer*, int depth)>& visitor, int depth = 0);

    // -----------------------------------------------------------------------
    // Bounds
    // -----------------------------------------------------------------------

    // Bounds of this layer's content in canvas coordinates
    LayerRect get_bounds() const;

    // Recursive bounds including all children
    LayerRect get_tree_bounds() const;

    // -----------------------------------------------------------------------
    // Operations
    // -----------------------------------------------------------------------

    // Deep-copy this layer and all its children/content
    std::unique_ptr<Layer> duplicate() const;

    // Flatten this group layer: composite all children into a single Pixel layer.
    // Only meaningful for Group layers.  Children are removed after flattening.
    PixelForgeErrorCode flatten();

    // Merge this layer onto the layer below it (in parent's children list).
    // This layer is removed from the parent after merging.
    PixelForgeErrorCode merge_down();

    // -----------------------------------------------------------------------
    // Dirty tracking
    // -----------------------------------------------------------------------
    void mark_dirty();
    bool is_dirty() const { return m_dirty; }
    void clear_dirty();
    void clear_dirty_recursive();

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------

    // Total pixel memory used by this layer (content + mask), not recursive
    size_t memory_usage() const;

    // Total pixel memory used by this layer and all descendants
    size_t memory_usage_recursive() const;

    // Create a new Pixel layer from an existing image (takes ownership)
    static std::unique_ptr<Layer> from_image(const std::string& name,
                                              std::unique_ptr<Image> img);

    // Create a new Group layer
    static std::unique_ptr<Layer> make_group(const std::string& name);

    // Create a new Adjustment layer
    static std::unique_ptr<Layer> make_adjustment(const std::string& name,
                                                   const AdjustmentParams& params);

private:
    std::vector<std::unique_ptr<Layer>> m_children;
    bool m_dirty = true;

    // Helper: propagate dirty flag up to root
    void propagate_dirty();
};

} // namespace PixelForge
