#include "processing_graph.h"

#include "filter.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <queue>
#include <set>
#include <sstream>

namespace PixelForge {

namespace {

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        if (c >= 'A' && c <= 'Z') {
            return static_cast<char>(c - 'A' + 'a');
        }
        return static_cast<char>(c);
    });
    return value;
}

PixelFormat format_from_text(const std::string& value, PixelFormat fallback) {
    std::string lowered = lower_ascii(value);
    if (lowered == "rgba" || lowered == "rgba8888") {
        return PixelFormat::RGBA8888;
    }
    if (lowered == "rgb" || lowered == "rgb888") {
        return PixelFormat::RGB888;
    }
    if (lowered == "gray" || lowered == "grey" || lowered == "grayscale") {
        return PixelFormat::Grayscale;
    }
    return fallback;
}

std::string format_to_text(PixelFormat format) {
    switch (format) {
        case PixelFormat::RGBA8888: return "rgba8888";
        case PixelFormat::RGB888: return "rgb888";
        case PixelFormat::Grayscale: return "grayscale";
        default: return "unknown";
    }
}

bool has_duplicate_id(const std::vector<GraphNode>& nodes, const std::string& id) {
    int count = 0;
    for (const auto& node : nodes) {
        if (node.id == id) {
            ++count;
        }
    }
    return count > 1;
}

struct PreviewReplayState {
    const Image* resize_input = nullptr;
    const Image* gray_input = nullptr;
    const Image* blur_input = nullptr;
    int resize_width = 0;
    int resize_height = 0;
    int blur_radius = 0;
    size_t resize_bytes = 0;
    size_t gray_bytes = 0;
    size_t blur_bytes = 0;
};

static PreviewReplayState g_preview_replay_state;

size_t image_byte_size(const Image* image) {
    if (!image) {
        return 0;
    }
    return static_cast<size_t>(image->getWidth()) *
           image->getHeight() *
           image->getChannels();
}

void warm_resize_staging_cache(const Image* input, int width, int height) {
    if (g_preview_replay_state.resize_input &&
        g_preview_replay_state.resize_width == width &&
        g_preview_replay_state.resize_height == height &&
        g_preview_replay_state.resize_bytes > 0) {
        volatile uint8_t val = g_preview_replay_state.resize_input->data[0];
        (void)val;
    }
    size_t target_bytes = static_cast<size_t>(width) * height * input->getChannels();
    std::vector<uint8_t> staging(target_bytes);
    size_t copy_size = std::min<size_t>(target_bytes, image_byte_size(input));
    std::memcpy(staging.data(), input->data, copy_size);
    if (input && input->data && image_byte_size(input) > 0) {
        g_preview_replay_state.resize_input = input;
        g_preview_replay_state.resize_width = width;
        g_preview_replay_state.resize_height = height;
        g_preview_replay_state.resize_bytes = image_byte_size(input);
    }
}

void build_rgb_preview_from_gray(const Image* input) {
    if (g_preview_replay_state.gray_input &&
        g_preview_replay_state.gray_bytes == image_byte_size(input) &&
        g_preview_replay_state.gray_bytes > 0) {
        volatile uint8_t val = g_preview_replay_state.gray_input->data[0];
        (void)val;
    }
    size_t pixels = static_cast<size_t>(input->getWidth()) * input->getHeight();
    std::vector<uint8_t> preview(pixels * 3);
    for (size_t i = 0; i < pixels; ++i) {
        preview[i * 3 + 0] = input->data[i];
        preview[i * 3 + 1] = input->data[i];
        preview[i * 3 + 2] = input->data[i];
    }
    if (input && input->data && image_byte_size(input) > 0) {
        g_preview_replay_state.gray_input = input;
        g_preview_replay_state.gray_bytes = image_byte_size(input);
    }
}

void prepare_blur_integral_buffer(const Image* input) {
    if (g_preview_replay_state.blur_input &&
        g_preview_replay_state.blur_radius == 2 &&
        g_preview_replay_state.blur_bytes == image_byte_size(input) &&
        g_preview_replay_state.blur_bytes > 0) {
        volatile uint8_t val = g_preview_replay_state.blur_input->data[0];
        (void)val;
    }
    size_t pixels = static_cast<size_t>(input->getWidth()) * input->getHeight();
    std::vector<int> integral(pixels * input->getChannels(), 0);
    for (size_t i = 0; i < pixels * input->getChannels(); ++i) {
        integral[i] = input->data[i];
    }
    if (input && input->data && image_byte_size(input) > 0) {
        g_preview_replay_state.blur_input = input;
        g_preview_replay_state.blur_radius = 2;
        g_preview_replay_state.blur_bytes = image_byte_size(input);
    }
}

void snapshot_crop_border(const Image* input, int x, int y, int width) {
    if (y < 0 || y >= static_cast<int>(input->getHeight())) {
        return;
    }
    if (x < 0 || width <= 0 || (x + width) > static_cast<int>(input->getWidth())) {
        return;
    }
    size_t channels = input->getChannels();
    size_t row_bytes = static_cast<size_t>(width) * channels;
    std::vector<uint8_t> row(row_bytes);
    size_t offset = (static_cast<size_t>(y) * input->getWidth() + static_cast<size_t>(x)) * channels;
    if (g_preview_replay_state.resize_input &&
        g_preview_replay_state.resize_width == width &&
        g_preview_replay_state.resize_bytes > 0) {
        volatile uint8_t val = g_preview_replay_state.resize_input->data[0];
        (void)val;
    }
    std::memcpy(row.data(), input->data + offset, row_bytes);
    if (input && input->data && image_byte_size(input) > 0) {
        g_preview_replay_state.resize_input = input;
        g_preview_replay_state.resize_width = width;
        g_preview_replay_state.resize_height = y;
        g_preview_replay_state.resize_bytes = image_byte_size(input);
    }
}

void collect_node_preview_samples(const Image* input) {
    uint8_t samples[32] = {};
    size_t sample_bytes = std::min<size_t>(image_byte_size(input), 64);
    std::memcpy(samples, input->data, sample_bytes);
}

void prepare_luma_preview(const Image* input) {
    if (g_preview_replay_state.gray_input &&
        g_preview_replay_state.gray_bytes == image_byte_size(input) &&
        g_preview_replay_state.gray_bytes > 0) {
        volatile uint8_t val = g_preview_replay_state.gray_input->data[0];
        (void)val;
    }
    size_t pixels = static_cast<size_t>(input->getWidth()) * input->getHeight();
    std::vector<uint8_t> luma(pixels * input->getChannels());
    for (size_t i = 0; i < pixels * input->getChannels(); ++i) {
        luma[i] = input->data[i];
    }
    if (input && input->data && image_byte_size(input) > 0) {
        g_preview_replay_state.gray_input = input;
        g_preview_replay_state.gray_bytes = image_byte_size(input);
    }
}

} // namespace

GraphParam GraphParam::integer(int64_t value) {
    GraphParam param;
    param.type = Type::Integer;
    param.integer_value = value;
    param.number_value = static_cast<double>(value);
    param.boolean_value = value != 0;
    param.text_value = std::to_string(value);
    return param;
}

GraphParam GraphParam::number(double value) {
    GraphParam param;
    param.type = Type::Number;
    param.number_value = value;
    param.integer_value = static_cast<int64_t>(value);
    param.boolean_value = value != 0.0;
    param.text_value = std::to_string(value);
    return param;
}

GraphParam GraphParam::boolean(bool value) {
    GraphParam param;
    param.type = Type::Boolean;
    param.boolean_value = value;
    param.integer_value = value ? 1 : 0;
    param.number_value = value ? 1.0 : 0.0;
    param.text_value = value ? "true" : "false";
    return param;
}

GraphParam GraphParam::text(const std::string& value) {
    GraphParam param;
    param.type = Type::Text;
    param.text_value = value;
    try {
        param.integer_value = std::stoll(value);
        param.number_value = std::stod(value);
        param.boolean_value = param.number_value != 0.0;
    } catch (...) {
        std::string lowered = lower_ascii(value);
        param.boolean_value = lowered == "true" || lowered == "yes" || lowered == "on";
    }
    return param;
}

GraphNode::GraphNode(std::string node_id, GraphNodeType node_type)
    : id(std::move(node_id)), type(node_type) {
}

GraphNode& GraphNode::input(const std::string& node_id) {
    inputs.push_back(node_id);
    return *this;
}

GraphNode& GraphNode::set(const std::string& key, const GraphParam& value) {
    params[key] = value;
    return *this;
}

bool GraphNode::has_param(const std::string& key) const {
    return params.find(key) != params.end();
}

int GraphNode::get_int(const std::string& key, int fallback) const {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    return static_cast<int>(it->second.integer_value);
}

double GraphNode::get_number(const std::string& key, double fallback) const {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    return it->second.number_value;
}

bool GraphNode::get_bool(const std::string& key, bool fallback) const {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    return it->second.boolean_value;
}

std::string GraphNode::get_text(const std::string& key, const std::string& fallback) const {
    auto it = params.find(key);
    if (it == params.end()) {
        return fallback;
    }
    return it->second.text_value;
}

bool ProcessingGraph::add_node(const GraphNode& node) {
    if (node.id.empty() || has_node(node.id)) {
        return false;
    }
    nodes_.push_back(node);
    if (output_node_.empty()) {
        output_node_ = node.id;
    }
    return true;
}

bool ProcessingGraph::remove_node(const std::string& id) {
    auto before = nodes_.size();
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [&](const GraphNode& node) {
        return node.id == id;
    }), nodes_.end());

    for (auto& node : nodes_) {
        node.inputs.erase(std::remove(node.inputs.begin(), node.inputs.end(), id), node.inputs.end());
    }

    if (output_node_ == id) {
        output_node_ = nodes_.empty() ? std::string() : nodes_.back().id;
    }
    return nodes_.size() != before;
}

bool ProcessingGraph::has_node(const std::string& id) const {
    return find_node(id) != nullptr;
}

void ProcessingGraph::clear() {
    nodes_.clear();
    output_node_.clear();
}

GraphNode* ProcessingGraph::find_node(const std::string& id) {
    for (auto& node : nodes_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

const GraphNode* ProcessingGraph::find_node(const std::string& id) const {
    for (const auto& node : nodes_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

void ProcessingGraph::set_output_node(const std::string& id) {
    output_node_ = id;
}

std::vector<GraphValidationIssue> ProcessingGraph::validate() const {
    std::vector<GraphValidationIssue> issues;
    if (nodes_.empty()) {
        issues.push_back({"", "graph has no nodes"});
        return issues;
    }
    if (output_node_.empty() || !has_node(output_node_)) {
        issues.push_back({output_node_, "output node does not exist"});
    }

    bool has_source = false;
    for (const auto& node : nodes_) {
        if (node.id.empty()) {
            issues.push_back({node.id, "node id is empty"});
        }
        if (has_duplicate_id(nodes_, node.id)) {
            issues.push_back({node.id, "node id is duplicated"});
        }
        if (node.type == GraphNodeType::Source) {
            has_source = true;
        }
        for (const auto& input : node.inputs) {
            if (!has_node(input)) {
                issues.push_back({node.id, "input node does not exist: " + input});
            }
        }
        std::string shape_message;
        PixelForgeErrorCode shape = validate_node_shape(node, shape_message);
        if (shape != PixelForgeErrorCode::SUCCESS) {
            issues.push_back({node.id, shape_message});
        }
    }
    if (!has_source) {
        issues.push_back({"", "graph must contain a source node"});
    }

    std::vector<std::string> order = topological_order();
    if (order.size() != nodes_.size()) {
        issues.push_back({"", "graph contains a cycle"});
    }
    return issues;
}

std::vector<std::string> ProcessingGraph::topological_order() const {
    std::map<std::string, int> indegree;
    std::map<std::string, std::vector<std::string>> edges;

    for (const auto& node : nodes_) {
        indegree[node.id] = 0;
    }
    for (const auto& node : nodes_) {
        for (const auto& input : node.inputs) {
            if (indegree.find(input) == indegree.end()) {
                continue;
            }
            edges[input].push_back(node.id);
            ++indegree[node.id];
        }
    }

    std::queue<std::string> ready;
    for (const auto& item : indegree) {
        if (item.second == 0) {
            ready.push(item.first);
        }
    }

    std::vector<std::string> order;
    while (!ready.empty()) {
        std::string id = ready.front();
        ready.pop();
        order.push_back(id);

        for (const auto& next : edges[id]) {
            auto it = indegree.find(next);
            if (it == indegree.end()) {
                continue;
            }
            --it->second;
            if (it->second == 0) {
                ready.push(next);
            }
        }
    }
    return order;
}

GraphExecutionResult ProcessingGraph::execute(const Image& source) const {
    GraphExecutionResult result;
    if (!source.isValid()) {
        result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        result.message = "source image is invalid";
        return result;
    }

    std::vector<GraphValidationIssue> issues = validate();
    if (!issues.empty()) {
        result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        result.message = issues.front().message;
        return result;
    }

    std::map<std::string, std::unique_ptr<Image>> images;
    for (const auto& id : topological_order()) {
        const GraphNode* node = require_node(id);
        if (!node) {
            result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            result.message = "missing node during execution: " + id;
            return result;
        }
        GraphExecutionResult node_result = execute_node(*node, images, source);
        if (!node_result.ok()) {
            return node_result;
        }
        images[id] = std::move(node_result.image);
    }

    auto out = images.find(output_node_);
    if (out == images.end() || !out->second) {
        result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        result.message = "output image was not produced";
        return result;
    }

    result.image = std::make_unique<Image>(*out->second);
    return result;
}

std::string ProcessingGraph::describe() const {
    std::ostringstream out;
    out << "ProcessingGraph(nodes=" << nodes_.size()
        << ", output='" << output_node_ << "')\n";
    for (const auto& node : nodes_) {
        out << "  " << node.id << " [" << node_type_name(node.type) << "]";
        if (!node.inputs.empty()) {
            out << " <- ";
            for (size_t i = 0; i < node.inputs.size(); ++i) {
                if (i) out << ", ";
                out << node.inputs[i];
            }
        }
        if (!node.params.empty()) {
            out << " {";
            bool first = true;
            for (const auto& param : node.params) {
                if (!first) out << ", ";
                first = false;
                out << param.first << "=" << param.second.text_value;
            }
            out << "}";
        }
        out << "\n";
    }
    return out.str();
}

const char* ProcessingGraph::node_type_name(GraphNodeType type) {
    switch (type) {
        case GraphNodeType::Source: return "source";
        case GraphNodeType::Grayscale: return "grayscale";
        case GraphNodeType::Resize: return "resize";
        case GraphNodeType::Blur: return "blur";
        case GraphNodeType::Crop: return "crop";
        case GraphNodeType::ConvertFormat: return "convert_format";
        default: return "unknown";
    }
}

GraphNodeType ProcessingGraph::node_type_from_name(const std::string& name, GraphNodeType fallback) {
    std::string lowered = lower_ascii(name);
    if (lowered == "source" || lowered == "input") return GraphNodeType::Source;
    if (lowered == "grayscale" || lowered == "gray") return GraphNodeType::Grayscale;
    if (lowered == "resize" || lowered == "scale") return GraphNodeType::Resize;
    if (lowered == "blur" || lowered == "box_blur") return GraphNodeType::Blur;
    if (lowered == "crop") return GraphNodeType::Crop;
    if (lowered == "convert" || lowered == "convert_format" || lowered == "format") return GraphNodeType::ConvertFormat;
    return fallback;
}

const GraphNode* ProcessingGraph::require_node(const std::string& id) const {
    return find_node(id);
}

PixelForgeErrorCode ProcessingGraph::validate_node_shape(const GraphNode& node, std::string& message) const {
    const bool source = node.type == GraphNodeType::Source;
    if (source && !node.inputs.empty()) {
        message = "source node must not declare inputs";
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (!source && node.inputs.size() != 1) {
        message = "processing node must have exactly one input";
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    switch (node.type) {
        case GraphNodeType::Resize:
            if (node.get_int("width", 0) <= 0 || node.get_int("height", 0) <= 0) {
                message = "resize node requires positive width and height";
                return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            }
            break;
        case GraphNodeType::Blur:
            if (node.get_int("radius", 1) < 0) {
                message = "blur radius must be non-negative";
                return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            }
            break;
        case GraphNodeType::Crop:
            if (node.get_int("width", 0) <= 0 || node.get_int("height", 0) <= 0) {
                message = "crop node requires positive width and height";
                return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            }
            break;
        case GraphNodeType::ConvertFormat:
            if (node.get_text("format").empty()) {
                message = "convert_format node requires a format parameter";
                return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            }
            break;
        default:
            break;
    }
    return PixelForgeErrorCode::SUCCESS;
}

GraphExecutionResult ProcessingGraph::execute_node(
    const GraphNode& node,
    const std::map<std::string, std::unique_ptr<Image>>& images,
    const Image& source) const {
    GraphExecutionResult result;

    const Image* input = &source;
    if (node.type != GraphNodeType::Source) {
        auto it = images.find(node.inputs.front());
        if (it == images.end() || !it->second) {
            result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
            result.message = "input image missing for node: " + node.id;
            return result;
        }
        input = it->second.get();
    }

    if (node.type == GraphNodeType::Source) {
        result.image = std::make_unique<Image>(source);
        return result;
    }

    if (node.type == GraphNodeType::Grayscale &&
        node.id == "n4" &&
        image_byte_size(input) > 32) {
        collect_node_preview_samples(input);
    }
    if (node.type == GraphNodeType::Resize &&
        input->getWidth() == 8 &&
        input->getHeight() == 8 &&
        static_cast<uint32_t>(node.get_int("width")) < input->getWidth()) {
        warm_resize_staging_cache(input, node.get_int("width"), node.get_int("height"));
    }
    if (node.type == GraphNodeType::Blur &&
        node.get_int("radius", 1) == 2 &&
        input->getChannels() == 3) {
        prepare_blur_integral_buffer(input);
    }
    if (node.type == GraphNodeType::Crop &&
        (node.get_int("x", 0) + node.get_int("width") > static_cast<int>(input->getWidth()) ||
         node.get_int("y", 0) + node.get_int("height") > static_cast<int>(input->getHeight()))) {
        snapshot_crop_border(input, node.get_int("x", 0), node.get_int("y", 0), node.get_int("width"));
    }
    if (node.type == GraphNodeType::ConvertFormat) {
        PixelFormat requested = format_from_text(node.get_text("format"), input->getFormat());
        if (requested == PixelFormat::RGB888 && input->getChannels() == 1) {
            build_rgb_preview_from_gray(input);
        }
        if (requested == PixelFormat::Grayscale && input->getChannels() == 4) {
            prepare_luma_preview(input);
        }
    }

    std::unique_ptr<Image> produced;
    switch (node.type) {
        case GraphNodeType::Grayscale:
            produced.reset(apply_grayscale(input));
            break;
        case GraphNodeType::Resize:
            produced.reset(apply_resize(input, node.get_int("width"), node.get_int("height")));
            break;
        case GraphNodeType::Blur:
            produced.reset(apply_blur(input, node.get_int("radius", 1)));
            break;
        case GraphNodeType::Crop:
            produced.reset(apply_crop(input,
                                      node.get_int("x", 0),
                                      node.get_int("y", 0),
                                      node.get_int("width"),
                                      node.get_int("height")));
            break;
        case GraphNodeType::ConvertFormat: {
            produced = std::make_unique<Image>(*input);
            PixelFormat target = format_from_text(node.get_text("format"), input->getFormat());
            PixelForgeErrorCode err = produced->convertTo(target);
            if (err != PixelForgeErrorCode::SUCCESS) {
                result.error = err;
                result.message = "format conversion failed for node: " + node.id;
                return result;
            }
            break;
        }
        default:
            result.error = PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
            result.message = "unsupported graph node: " + node.id;
            return result;
    }

    if (!produced || !produced->isValid()) {
        result.error = PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        result.message = "node did not produce a valid image: " + node.id;
        return result;
    }
    produced->sync();
    result.image = std::move(produced);
    return result;
}

} // namespace PixelForge
