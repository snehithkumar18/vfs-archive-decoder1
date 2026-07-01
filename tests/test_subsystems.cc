#include "../src/blend.h"
#include "../src/color_convert.h"
#include "../src/expression.h"
#include "../src/expression_eval.h"
#include "../src/animation.h"
#include "../src/manifest.h"
#include "../src/memory_stream.h"
#include "../src/processing_graph.h"
#include "../src/tile.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace PixelForge;

namespace {

bool near(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) <= epsilon;
}

void test_memory_stream() {
    MemoryStream stream;
    assert(stream.write_u16_le(0x1234));
    assert(stream.write_string("pf"));
    assert(stream.size() == 4);
    assert(stream.rewind());

    uint16_t value = 0;
    std::string marker;
    assert(stream.read_u16_le(value));
    assert(value == 0x1234);
    assert(stream.read_string(marker, 2));
    assert(marker == "pf");

    MemoryStream clone = stream.clone();
    assert(clone.seek(0, SeekOrigin::Begin));
    assert(clone.write_u8(0x99));
    assert(stream.get_data()[0] == 0x34);
    assert(clone.get_data()[0] == 0x99);
}

void test_color_management() {
    ColorSpace srgb = ColorSpace::sRGB();
    ColorSpace linear = ColorSpace::LinearRGB();

    ColorF red(1.0f, 0.0f, 0.0f, 0.75f);
    LabColorF lab = ColorConverter::rgb_to_lab(red, srgb);
    ColorF roundtrip = ColorConverter::lab_to_rgb(lab, srgb);
    roundtrip.clamp();
    assert(near(roundtrip.r, 1.0f));
    assert(near(roundtrip.g, 0.0f));
    assert(near(roundtrip.b, 0.0f));

    ColorConverter converter(srgb, linear);
    ColorF mid_gray(0.5f, 0.5f, 0.5f, 1.0f);
    converter.convert(mid_gray);
    assert(mid_gray.r > 0.20f && mid_gray.r < 0.23f);
    assert(near(mid_gray.r, mid_gray.g));
    assert(near(mid_gray.g, mid_gray.b));
}

void test_blending() {
    uint8_t base[4] = {100, 100, 100, 255};
    uint8_t overlay[4] = {200, 50, 50, 128};
    blend_pixel(base, overlay, BlendMode::Normal, 1.0f, 4);
    assert(base[0] > 100);
    assert(base[1] < 100);
    assert(base[2] < 100);
    assert(base[3] == 255);

    assert(blend_mode_from_name("multiply") == BlendMode::Multiply);
    assert(blend_channel(0.5f, 0.5f, BlendMode::Screen) > 0.5f);
}

void test_manifest_parser() {
    const std::string text =
        "{\n"
        "  \"base_dir\": \"assets\",\n"
        "  \"max_threads\": 2,\n"
        "  \"fail_fast\": true,\n"
        "  \"jobs\": [\n"
        "    {\"name\":\"thumb\", \"input\":\"a.bmp\", \"output\":\"a.png\", \"operations\":[\"resize\", \"sharpen\"], \"priority\":5}\n"
        "  ]\n"
        "}\n";

    ManifestValue root = ManifestParser::parse(text);
    assert(root.is_object());
    assert(root["jobs"].is_array());
    assert(root["jobs"].size() == 1);

    Manifest manifest = Manifest::from_value(root);
    assert(manifest.base_dir == "assets");
    assert(manifest.max_threads == 2);
    assert(manifest.fail_fast);
    assert(manifest.job_count() == 1);
    assert(manifest.jobs[0].operations.size() == 2);
    assert(manifest.validate().empty());
}

void test_expression_parser() {
    ExpressionParser parser("gain = 2 + 3 * 4; gain > 10 ? gain : 0");
    std::unique_ptr<ASTNode> ast = parser.parse();
    assert(ast != nullptr);
    assert(!parser.has_error());
    assert(ast->type == ASTType::StatementList);
    assert(ast->node_count() >= 10);

    ExpressionContext context;
    ExpressionEvaluator evaluator(context);
    double result = evaluator.evaluate(ast.get());
    assert(!evaluator.has_error());
    assert(result == 14.0);
    assert(context.has_variable("gain"));
    assert(context.get_variable("gain") == 14.0);

    double shaped = evaluator.evaluate("x = clamp(1.25, 0, 1); mix(10, 20, x)");
    assert(!evaluator.has_error());
    assert(shaped == 20.0);

    double trig = evaluator.evaluate("round(sin(pi / 2) * 100)");
    assert(!evaluator.has_error());
    assert(trig == 100.0);

    evaluator.evaluate("1 / 0");
    assert(evaluator.has_error());
}

void test_tiles() {
    Image image(4, 4, PixelFormat::RGBA8888);
    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            size_t idx = (static_cast<size_t>(y) * image.width + x) * image.channels;
            image.data[idx + 0] = static_cast<uint8_t>(x * 20);
            image.data[idx + 1] = static_cast<uint8_t>(y * 30);
            image.data[idx + 2] = 90;
            image.data[idx + 3] = 255;
        }
    }

    TileDescriptor desc;
    desc.tile_x = 0;
    desc.tile_y = 0;
    desc.region = {1, 1, 2, 2};
    desc.halo_region = desc.region.expanded(1).clamped(4, 4);
    desc.halo_size = 1;

    Tile tile(desc, 4);
    assert(tile.copy_from_image(image) == PixelForgeErrorCode::SUCCESS);
    Pixel p = tile.get_pixel(1, 1);
    assert(p.r == 20);
    assert(p.g == 30);

    tile.set_pixel(1, 1, Pixel{255, 0, 0, 255});
    assert(tile.write_to_image(image) == PixelForgeErrorCode::SUCCESS);
    size_t dst = (static_cast<size_t>(1) * image.width + 1) * image.channels;
    assert(image.data[dst] == 255);
}

void test_processing_graph() {
    Image source(4, 4, PixelFormat::RGBA8888);
    for (uint32_t y = 0; y < source.height; ++y) {
        for (uint32_t x = 0; x < source.width; ++x) {
            source.set_pixel(x, y, Pixel{
                static_cast<uint8_t>(20 + x * 30),
                static_cast<uint8_t>(40 + y * 20),
                90,
                255
            });
        }
    }

    ProcessingGraph graph;
    assert(graph.add_node(GraphNode("src", GraphNodeType::Source)));
    GraphNode crop("crop", GraphNodeType::Crop);
    crop.input("src")
        .set("x", GraphParam::integer(1))
        .set("y", GraphParam::integer(1))
        .set("width", GraphParam::integer(2))
        .set("height", GraphParam::integer(2));
    assert(graph.add_node(crop));

    GraphNode gray("gray", GraphNodeType::Grayscale);
    gray.input("crop");
    assert(graph.add_node(gray));

    GraphNode resize("small", GraphNodeType::Resize);
    resize.input("gray")
        .set("width", GraphParam::integer(1))
        .set("height", GraphParam::integer(1));
    assert(graph.add_node(resize));
    graph.set_output_node("small");

    assert(graph.validate().empty());
    std::vector<std::string> order = graph.topological_order();
    assert(order.size() == 4);
    assert(order.front() == "src");

    GraphExecutionResult result = graph.execute(source);
    assert(result.ok());
    assert(result.image->getWidth() == 1);
    assert(result.image->getHeight() == 1);
    assert(result.image->getChannels() == 4);

    ProcessingGraph invalid;
    invalid.add_node(GraphNode("a", GraphNodeType::Grayscale).input("missing"));
    invalid.set_output_node("a");
    assert(!invalid.validate().empty());
}

std::unique_ptr<Image> solid_image(uint32_t width, uint32_t height, Pixel color) {
    auto image = std::make_unique<Image>(width, height, PixelFormat::RGBA8888);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            image->set_pixel(x, y, color);
        }
    }
    image->sync();
    return image;
}

void test_animation_timeline() {
    Animation anim(4, 4);
    anim.background_color = Pixel{0, 0, 0, 255};
    PixelForgeErrorCode first_add =
        anim.add_frame(solid_image(4, 4, Pixel{255, 0, 0, 255}), 50, FrameDisposal::None);
    PixelForgeErrorCode second_add =
        anim.add_frame(solid_image(2, 2, Pixel{0, 255, 0, 255}), 75, FrameDisposal::Background);
    assert(first_add == PixelForgeErrorCode::SUCCESS);
    assert(second_add == PixelForgeErrorCode::SUCCESS);
    AnimationFrame* overlay = anim.get_frame(1);
    assert(overlay != nullptr);
    overlay->x = 1;
    overlay->y = 1;

    assert(anim.frame_count() == 2);
    assert(anim.total_duration_ms() == 125);
    assert(anim.frame_start_time_ms(1) == 50);
    assert(anim.get_frame_at_time(60) == 1);
    assert(anim.memory_usage() > 0);

    Animation copy = anim.clone();
    assert(copy.frame_count() == 2);
    copy.set_uniform_delay(40);
    assert(copy.total_duration_ms() == 80);

    Animation range = anim.extract_range(1, 2);
    assert(range.frame_count() == 1);
    Animation appended(4, 4);
    appended.append(std::move(range));
    assert(appended.frame_count() == 1);

    AnimationPlayer player(&anim);
    Image canvas(4, 4, PixelFormat::RGBA8888);
    player.render_current(canvas);
    assert(canvas.get_pixel(0, 0).r == 255);
    assert(player.advance(60));
    player.render_current(canvas);
    assert(player.current_frame_index() == 1);
    assert(canvas.get_pixel(1, 1).g == 255);
}

} // namespace

int main() {
    test_memory_stream();
    test_color_management();
    test_blending();
    test_manifest_parser();
    test_expression_parser();
    test_tiles();
    test_processing_graph();
    test_animation_timeline();

    std::cout << "Subsystem integration tests passed.\n";
    return 0;
}
