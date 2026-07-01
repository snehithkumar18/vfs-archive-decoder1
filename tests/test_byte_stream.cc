#include "../src/byte_stream.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace PixelForge;

static void test_checked_math() {
    size_t value = 0;
    assert(checked_add<size_t>(10, 20, value) && value == 30);
    assert(!checked_add(std::numeric_limits<size_t>::max(), size_t{1}, value));
    assert(checked_mul<size_t>(7, 9, value) && value == 63);
    assert(!checked_mul(std::numeric_limits<size_t>::max(), size_t{2}, value));
    assert(checked_align_up<size_t>(13, 8, value) && value == 16);
    assert(!checked_align_up<size_t>(13, 3, value));

    ImageLayout layout;
    assert(checked_image_layout(13, 5, 3, 4, layout));
    assert(layout.row_bytes == 40);
    assert(layout.pixel_bytes == 200);
}

static void test_little_endian_round_trip() {
    ByteWriter writer;
    assert(writer.write_u8(0x12));
    assert(writer.write_u16(0x3456));
    assert(writer.write_u24(0x789ABC));
    assert(writer.write_u32(0xDEF01234));
    assert(writer.write_u64(0x0123456789ABCDEFULL));

    ByteReader reader(writer.data());
    uint8_t a = 0;
    uint16_t b = 0;
    uint32_t c = 0, d = 0;
    uint64_t e = 0;
    assert(reader.read_u8(a) && a == 0x12);
    assert(reader.read_u16(b) && b == 0x3456);
    assert(reader.read_u24(c) && c == 0x789ABC);
    assert(reader.read_u32(d) && d == 0xDEF01234);
    assert(reader.read_u64(e) && e == 0x0123456789ABCDEFULL);
    assert(reader.eof());
}

static void test_big_endian_round_trip() {
    ByteWriter writer(ByteOrder::BigEndian);
    assert(writer.write_u16(0x1234));
    assert(writer.write_u32(0x56789ABC));
    assert(writer.write_i32(-123456));

    ByteReader reader(writer.data(), ByteOrder::BigEndian);
    uint16_t a = 0;
    uint32_t b = 0;
    int32_t c = 0;
    assert(reader.read_u16(a) && a == 0x1234);
    assert(reader.read_u32(b) && b == 0x56789ABC);
    assert(reader.read_i32(c) && c == -123456);
}

static void test_atomic_failures() {
    const std::vector<uint8_t> bytes{1, 2, 3};
    ByteReader reader(bytes);
    uint32_t value = 0;
    assert(!reader.read_u32(value));
    assert(reader.position() == 0);
    assert(!reader.skip(std::numeric_limits<size_t>::max()));
    assert(reader.position() == 0);
    assert(!reader.seek(4));
    assert(reader.position() == 0);
}

static void test_views_strings_and_patching() {
    ByteWriter writer;
    assert(writer.write_u32(0));
    assert(writer.write_string("pixel", true));
    assert(writer.align(8, 0xCC));
    assert(writer.patch_u32(0, static_cast<uint32_t>(writer.position())));

    ByteReader reader(writer.data());
    uint32_t total = 0;
    std::string name;
    assert(reader.read_u32(total));
    assert(total == writer.data().size());
    assert(reader.read_c_string(16, name));
    assert(name == "pixel");
    assert(reader.align(8));
    assert(reader.eof());

    ByteView whole(writer.data());
    ByteView name_view;
    assert(whole.subview(4, 5, name_view));
    assert(name_view.size() == 5 && name_view[0] == 'p');
    assert(!whole.subview(whole.size(), 1, name_view));
}

int main() {
    test_checked_math();
    test_little_endian_round_trip();
    test_big_endian_round_trip();
    test_atomic_failures();
    test_views_strings_and_patching();
    return 0;
}
