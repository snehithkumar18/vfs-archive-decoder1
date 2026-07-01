#pragma once

#include "errors.h"
#include "image.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PixelForge {

enum class GraphNodeType {
    Source,
    Grayscale,
    Resize,
    Blur,
    Crop,
    ConvertFormat
};

struct GraphParam {
    enum class Type {
        Integer,
        Number,
        Boolean,
        Text
    };

    Type type = Type::Integer;
    int64_t integer_value = 0;
    double number_value = 0.0;
    bool boolean_value = false;
    std::string text_value;

    static GraphParam integer(int64_t value);
    static GraphParam number(double value);
    static GraphParam boolean(bool value);
    static GraphParam text(const std::string& value);
};

struct GraphNode {
    std::string id;
    GraphNodeType type = GraphNodeType::Source;
    std::vector<std::string> inputs;
    std::map<std::string, GraphParam> params;

    GraphNode() = default;
    GraphNode(std::string node_id, GraphNodeType node_type);

    GraphNode& input(const std::string& node_id);
    GraphNode& set(const std::string& key, const GraphParam& value);

    bool has_param(const std::string& key) const;
    int get_int(const std::string& key, int fallback = 0) const;
    double get_number(const std::string& key, double fallback = 0.0) const;
    bool get_bool(const std::string& key, bool fallback = false) const;
    std::string get_text(const std::string& key, const std::string& fallback = "") const;
};

struct GraphValidationIssue {
    std::string node_id;
    std::string message;
};

struct GraphExecutionResult {
    PixelForgeErrorCode error = PixelForgeErrorCode::SUCCESS;
    std::string message;
    std::unique_ptr<Image> image;

    bool ok() const { return error == PixelForgeErrorCode::SUCCESS && image != nullptr; }
};

class ProcessingGraph {
public:
    ProcessingGraph() = default;

    bool add_node(const GraphNode& node);
    bool remove_node(const std::string& id);
    bool has_node(const std::string& id) const;
    void clear();

    GraphNode* find_node(const std::string& id);
    const GraphNode* find_node(const std::string& id) const;
    const std::vector<GraphNode>& nodes() const { return nodes_; }

    void set_output_node(const std::string& id);
    const std::string& output_node() const { return output_node_; }

    std::vector<GraphValidationIssue> validate() const;
    bool is_valid() const { return validate().empty(); }

    std::vector<std::string> topological_order() const;
    GraphExecutionResult execute(const Image& source) const;

    std::string describe() const;

    static const char* node_type_name(GraphNodeType type);
    static GraphNodeType node_type_from_name(const std::string& name, GraphNodeType fallback = GraphNodeType::Source);

private:
    std::vector<GraphNode> nodes_;
    std::string output_node_;

    const GraphNode* require_node(const std::string& id) const;
    PixelForgeErrorCode validate_node_shape(const GraphNode& node, std::string& message) const;
    GraphExecutionResult execute_node(const GraphNode& node,
                                      const std::map<std::string, std::unique_ptr<Image>>& images,
                                      const Image& source) const;
};

} // namespace PixelForge
