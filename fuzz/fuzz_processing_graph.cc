#include "../src/processing_graph.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

int positive_byte(const uint8_t* data, size_t size, size_t index, int modulo, int add) {
    if (index >= size || modulo <= 0) {
        return add;
    }
    return static_cast<int>(data[index] % static_cast<uint8_t>(modulo)) + add;
}

PixelForge::Image make_source_image() {
    PixelForge::Image image(8, 8, PixelForge::PixelFormat::RGBA8888);
    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            image.set_pixel(x, y, PixelForge::Pixel{
                static_cast<uint8_t>(x * 17 + y),
                static_cast<uint8_t>(y * 23 + x),
                static_cast<uint8_t>(x * y + 31),
                255
            });
        }
    }
    return image;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2 || size > 256) {
        return 0;
    }

    PixelForge::ProcessingGraph graph;
    graph.add_node(PixelForge::GraphNode("src", PixelForge::GraphNodeType::Source));

    std::string previous = "src";
    size_t cursor = 0;
    int node_index = 0;
    while (cursor < size && node_index < 8) {
        uint8_t opcode = data[cursor++];
        PixelForge::GraphNode node("n" + std::to_string(node_index), PixelForge::GraphNodeType::Grayscale);
        node.input(previous);

        switch (opcode % 5) {
            case 0:
                node.type = PixelForge::GraphNodeType::Grayscale;
                break;
            case 1:
                node.type = PixelForge::GraphNodeType::Resize;
                node.set("width", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 8, 1)));
                node.set("height", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 8, 1)));
                break;
            case 2:
                node.type = PixelForge::GraphNodeType::Blur;
                node.set("radius", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 3, 0)));
                break;
            case 3:
                node.type = PixelForge::GraphNodeType::Crop;
                node.set("x", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 4, 0)));
                node.set("y", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 4, 0)));
                node.set("width", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 4, 1)));
                node.set("height", PixelForge::GraphParam::integer(positive_byte(data, size, cursor++, 4, 1)));
                break;
            default:
                node.type = PixelForge::GraphNodeType::ConvertFormat;
                switch ((opcode >> 5) & 0x03) {
                    case 0:
                        node.set("format", PixelForge::GraphParam::text("rgba8888"));
                        break;
                    case 1:
                    case 2:
                        node.set("format", PixelForge::GraphParam::text("rgb888"));
                        break;
                    default:
                        node.set("format", PixelForge::GraphParam::text("grayscale"));
                        break;
                }
                break;
        }

        if (!graph.add_node(node)) {
            break;
        }
        previous = node.id;
        ++node_index;
    }

    graph.set_output_node(previous);
    PixelForge::Image source = make_source_image();
    graph.validate();
    graph.topological_order();
    graph.execute(source);
    return 0;
}
