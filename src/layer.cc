#include "layer.h"
#include "blend.h"
#include <algorithm>
#include <cstring>

namespace PixelForge {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Layer::Layer(const std::string& name, LayerType type)
    : name(name), type(type) {
}

// ---------------------------------------------------------------------------
// Child management
// ---------------------------------------------------------------------------

Layer* Layer::child_at(size_t index) const {
    if (index >= m_children.size()) return nullptr;
    return m_children[index].get();
}

Layer* Layer::add_child(std::unique_ptr<Layer> child) {
    if (!child) return nullptr;
    child->parent = this;
    Layer* raw = child.get();
    m_children.push_back(std::move(child));
    mark_dirty();
    return raw;
}

Layer* Layer::insert_child(size_t index, std::unique_ptr<Layer> child) {
    if (!child) return nullptr;
    child->parent = this;
    Layer* raw = child.get();

    // Clamp index to valid range
    if (index > m_children.size()) {
        index = m_children.size();
    }

    m_children.insert(m_children.begin() + static_cast<ptrdiff_t>(index),
                      std::move(child));
    mark_dirty();
    return raw;
}

std::unique_ptr<Layer> Layer::remove_child(size_t index) {
    if (index >= m_children.size()) return nullptr;

    auto child = std::move(m_children[index]);
    m_children.erase(m_children.begin() + static_cast<ptrdiff_t>(index));
    child->parent = nullptr;
    mark_dirty();
    return child;
}

PixelForgeErrorCode Layer::move_child(size_t from, size_t to) {
    if (from >= m_children.size() || to >= m_children.size()) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }
    if (from == to) return PixelForgeErrorCode::SUCCESS;

    // Extract the child
    auto child = std::move(m_children[from]);
    m_children.erase(m_children.begin() + static_cast<ptrdiff_t>(from));

    // Adjust target index if we removed before it
    if (to > from && to > 0) {
        // `to` is still valid because we want to insert at the position
        // as if the element hadn't been removed yet.  But after removal
        // the vector is 1 shorter, so we need to clamp.
        if (to > m_children.size()) {
            to = m_children.size();
        }
    }

    m_children.insert(m_children.begin() + static_cast<ptrdiff_t>(to),
                      std::move(child));
    mark_dirty();
    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// Tree navigation
// ---------------------------------------------------------------------------

int Layer::depth() const {
    int d = 0;
    const Layer* p = parent;
    while (p) {
        ++d;
        p = p->parent;
    }
    return d;
}

Layer* Layer::root() {
    Layer* r = this;
    while (r->parent) r = r->parent;
    return r;
}

const Layer* Layer::root() const {
    const Layer* r = this;
    while (r->parent) r = r->parent;
    return r;
}

int Layer::index_in_parent() const {
    if (!parent) return -1;
    for (size_t i = 0; i < parent->m_children.size(); ++i) {
        if (parent->m_children[i].get() == this) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

Layer* Layer::find_by_name(const std::string& search_name) {
    if (name == search_name) return this;
    for (auto& child : m_children) {
        Layer* found = child->find_by_name(search_name);
        if (found) return found;
    }
    return nullptr;
}

const Layer* Layer::find_by_name(const std::string& search_name) const {
    if (name == search_name) return this;
    for (const auto& child : m_children) {
        const Layer* found = child->find_by_name(search_name);
        if (found) return found;
    }
    return nullptr;
}

void Layer::collect_all(std::vector<Layer*>& out) {
    out.push_back(this);
    for (auto& child : m_children) {
        child->collect_all(out);
    }
}

void Layer::walk(const std::function<void(Layer*, int depth)>& visitor, int d) {
    visitor(this, d);
    for (auto& child : m_children) {
        child->walk(visitor, d + 1);
    }
}

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------

LayerRect Layer::get_bounds() const {
    if (content && content->isValid()) {
        return {position_x, position_y,
                static_cast<int>(content->width),
                static_cast<int>(content->height)};
    }
    // For group or empty layers, return a zero-size rect at the position
    return {position_x, position_y, 0, 0};
}

LayerRect Layer::get_tree_bounds() const {
    LayerRect bounds = get_bounds();

    for (const auto& child : m_children) {
        LayerRect child_bounds = child->get_tree_bounds();
        // Offset child bounds by this layer's position
        // (children are relative to the parent in the tree)
        bounds = bounds.unite(child_bounds);
    }

    return bounds;
}

// ---------------------------------------------------------------------------
// duplicate — deep copy
// ---------------------------------------------------------------------------

std::unique_ptr<Layer> Layer::duplicate() const {
    auto copy = std::make_unique<Layer>(name, type);

    copy->blend_mode   = blend_mode;
    copy->opacity      = opacity;
    copy->visible      = visible;
    copy->locked       = locked;
    copy->position_x   = position_x;
    copy->position_y   = position_y;
    copy->isolated     = isolated;
    copy->pass_through = pass_through;
    copy->adjustment   = adjustment;

    // Deep-copy content image
    if (content) {
        copy->content = std::make_unique<Image>(*content);
    }

    // Deep-copy mask image
    if (mask) {
        copy->mask = std::make_unique<Image>(*mask);
    }

    // Recursively duplicate children
    for (const auto& child : m_children) {
        auto child_copy = child->duplicate();
        child_copy->parent = copy.get();
        copy->m_children.push_back(std::move(child_copy));
    }

    copy->m_dirty = m_dirty;
    return copy;
}

// ---------------------------------------------------------------------------
// flatten — composite group children into a single pixel layer
// ---------------------------------------------------------------------------

PixelForgeErrorCode Layer::flatten() {
    if (type != LayerType::Group) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (m_children.empty()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Compute the bounding box of all children
    LayerRect bounds = {0, 0, 0, 0};
    bool first = true;
    for (const auto& child : m_children) {
        if (!child->visible) continue;
        LayerRect cb = child->get_bounds();
        if (cb.empty()) continue;
        if (first) {
            bounds = cb;
            first = false;
        } else {
            bounds = bounds.unite(cb);
        }
    }

    if (bounds.empty()) {
        // All children invisible or empty — create minimal 1x1
        bounds = {position_x, position_y, 1, 1};
    }

    // Allocate a new image for the flattened result
    auto result = std::make_unique<Image>(
        static_cast<uint32_t>(bounds.w),
        static_cast<uint32_t>(bounds.h),
        PixelFormat::RGBA8888);

    // Clear to transparent
    std::memset(result->data, 0,
                static_cast<size_t>(bounds.w) * bounds.h * 4);

    // Composite each visible child onto the result
    for (const auto& child : m_children) {
        if (!child->visible) continue;
        if (!child->content || !child->content->isValid()) continue;

        // Child position relative to the flattened result origin
        int rel_x = child->position_x - bounds.x;
        int rel_y = child->position_y - bounds.y;

        // Ensure child content is RGBA for blending
        Image src_copy = *child->content;
        if (src_copy.getFormat() != PixelFormat::RGBA8888) {
            src_copy.convertTo(PixelFormat::RGBA8888);
        }

        // Apply mask if present
        if (child->mask && child->mask->isValid()) {
            blend_region_masked(*result, src_copy, *child->mask,
                               rel_x, rel_y,
                               child->blend_mode, child->opacity);
        } else {
            blend_region(*result, src_copy,
                        rel_x, rel_y,
                        child->blend_mode, child->opacity);
        }
    }

    // Convert this layer to a Pixel layer
    type = LayerType::Pixel;
    content = std::move(result);
    position_x = bounds.x;
    position_y = bounds.y;

    // Remove all children
    m_children.clear();
    mark_dirty();

    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// merge_down — blend this layer onto the one below in parent's list
// ---------------------------------------------------------------------------

PixelForgeErrorCode Layer::merge_down() {
    if (!parent) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    int my_index = index_in_parent();
    if (my_index < 0 || my_index == 0) {
        // No layer below to merge into
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }

    Layer* below = parent->child_at(static_cast<size_t>(my_index - 1));
    if (!below) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }

    // Both layers need pixel content for merging
    if (!content || !content->isValid()) {
        // Nothing to merge — just remove this layer
        parent->remove_child(static_cast<size_t>(my_index));
        return PixelForgeErrorCode::SUCCESS;
    }

    // If the lower layer has no content, create an empty canvas for it
    if (!below->content || !below->content->isValid()) {
        // Use this layer's bounds to create the base
        LayerRect this_bounds = get_bounds();
        below->content = std::make_unique<Image>(
            static_cast<uint32_t>(this_bounds.w),
            static_cast<uint32_t>(this_bounds.h),
            PixelFormat::RGBA8888);
        std::memset(below->content->data, 0,
                    static_cast<size_t>(this_bounds.w) * this_bounds.h * 4);
        below->position_x = this_bounds.x;
        below->position_y = this_bounds.y;
    }

    // Compute merged bounds (union of both layers)
    LayerRect below_bounds = below->get_bounds();
    LayerRect this_bounds = get_bounds();
    LayerRect merged_bounds = below_bounds.unite(this_bounds);

    // If the merged bounds differ from below's current content, we need
    // to expand the lower layer's canvas
    if (merged_bounds.x != below_bounds.x ||
        merged_bounds.y != below_bounds.y ||
        merged_bounds.w != below_bounds.w ||
        merged_bounds.h != below_bounds.h) {

        auto expanded = std::make_unique<Image>(
            static_cast<uint32_t>(merged_bounds.w),
            static_cast<uint32_t>(merged_bounds.h),
            PixelFormat::RGBA8888);
        std::memset(expanded->data, 0,
                    static_cast<size_t>(merged_bounds.w) * merged_bounds.h * 4);

        // Copy below's content into the expanded canvas
        if (below->content->isValid()) {
            Image below_rgba = *below->content;
            if (below_rgba.getFormat() != PixelFormat::RGBA8888) {
                below_rgba.convertTo(PixelFormat::RGBA8888);
            }
            blend_region(*expanded, below_rgba,
                        below->position_x - merged_bounds.x,
                        below->position_y - merged_bounds.y,
                        BlendMode::Normal, 1.0f);
        }

        below->content = std::move(expanded);
        below->position_x = merged_bounds.x;
        below->position_y = merged_bounds.y;
    }

    // Ensure both are RGBA
    if (below->content->getFormat() != PixelFormat::RGBA8888) {
        below->content->convertTo(PixelFormat::RGBA8888);
    }
    Image src_copy = *content;
    if (src_copy.getFormat() != PixelFormat::RGBA8888) {
        src_copy.convertTo(PixelFormat::RGBA8888);
    }

    // Blend this layer onto below
    int rel_x = position_x - below->position_x;
    int rel_y = position_y - below->position_y;

    if (mask && mask->isValid()) {
        blend_region_masked(*below->content, src_copy, *mask,
                           rel_x, rel_y,
                           blend_mode, opacity);
    } else {
        blend_region(*below->content, src_copy,
                    rel_x, rel_y,
                    blend_mode, opacity);
    }

    below->mark_dirty();

    // Remove this layer from parent
    parent->remove_child(static_cast<size_t>(my_index));

    return PixelForgeErrorCode::SUCCESS;
}

// ---------------------------------------------------------------------------
// Dirty tracking
// ---------------------------------------------------------------------------

void Layer::mark_dirty() {
    m_dirty = true;
    propagate_dirty();
}

void Layer::clear_dirty() {
    m_dirty = false;
}

void Layer::clear_dirty_recursive() {
    m_dirty = false;
    for (auto& child : m_children) {
        child->clear_dirty_recursive();
    }
}

void Layer::propagate_dirty() {
    // Propagate dirty flag up to the root so that ancestors know
    // their cached composites are stale
    Layer* p = parent;
    while (p) {
        p->m_dirty = true;
        p = p->parent;
    }
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

size_t Layer::memory_usage() const {
    size_t mem = 0;
    if (content && content->isValid()) {
        mem += static_cast<size_t>(content->width) * content->height * content->channels;
    }
    if (mask && mask->isValid()) {
        mem += static_cast<size_t>(mask->width) * mask->height * mask->channels;
    }
    return mem;
}

size_t Layer::memory_usage_recursive() const {
    size_t mem = memory_usage();
    for (const auto& child : m_children) {
        mem += child->memory_usage_recursive();
    }
    return mem;
}

std::unique_ptr<Layer> Layer::from_image(const std::string& name,
                                          std::unique_ptr<Image> img) {
    auto layer = std::make_unique<Layer>(name, LayerType::Pixel);
    layer->content = std::move(img);
    return layer;
}

std::unique_ptr<Layer> Layer::make_group(const std::string& name) {
    auto layer = std::make_unique<Layer>(name, LayerType::Group);
    return layer;
}

std::unique_ptr<Layer> Layer::make_adjustment(const std::string& name,
                                               const AdjustmentParams& params) {
    auto layer = std::make_unique<Layer>(name, LayerType::Adjustment);
    layer->adjustment = params;
    return layer;
}

} // namespace PixelForge
