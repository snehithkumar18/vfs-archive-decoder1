#include "../src/string_utils.h"
#include "../src/config_parser.h"
#include "../src/thread_pool.h"
#include "../src/metadata.h"
#include "../src/errors.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <thread>
#include <chrono>
#include <future>
#include <random>

using namespace PixelForge;

void TestStringUtils() {
    using namespace PixelForge::StringUtils;
    std::cout << "Running extended StringUtils tests...\n";

    // 1. Base64 Encoding/Decoding
    {
        std::string orig = "Hello PixelForge base64 test!";
        std::string encoded = Base64Encode(orig);
        assert(encoded == "SGVsbG8gUGl4ZWxGb3JnZSBiYXNlNjQgdGVzdCE=");
        
        std::vector<uint8_t> decoded = Base64Decode(encoded);
        std::string decoded_str(decoded.begin(), decoded.end());
        assert(decoded_str == orig);

        // Standard edge cases
        assert(Base64Encode("") == "");
        assert(Base64Decode("") == std::vector<uint8_t>{});
        
        // Single characters
        assert(Base64Encode("A") == "QQ==");
        assert(Base64Decode("QQ==") == std::vector<uint8_t>{'A'});
        
        assert(Base64Encode("AB") == "QUI=");
        assert(Base64Decode("QUI=") == std::vector<uint8_t>{'A', 'B'});

        assert(Base64Encode("ABC") == "QUJD");
        assert(Base64Decode("QUJD") == std::vector<uint8_t>{'A', 'B', 'C'});

        // Invalid base64 check
        assert(Base64Decode("Invalid$$$") == std::vector<uint8_t>{});
        assert(Base64Decode("QUJDa") == std::vector<uint8_t>{}); // invalid padding length
    }

    // 2. Hex Encoding/Decoding
    {
        std::vector<uint8_t> bytes = {0x00, 0x1A, 0xFF, 0x9B, 0x05, 0x7E};
        std::string hex_lower = HexEncode(bytes, false);
        std::string hex_upper = HexEncode(bytes, true);
        assert(hex_lower == "001aff9b057e");
        assert(hex_upper == "001AFF9B057E");

        std::vector<uint8_t> decoded_hex = HexDecode(hex_lower);
        assert(decoded_hex == bytes);
        
        std::vector<uint8_t> decoded_hex_pref = HexDecode("0x" + hex_upper);
        assert(decoded_hex_pref == bytes);

        // Edge cases
        assert(HexEncode({}, false) == "");
        assert(HexDecode("") == std::vector<uint8_t>{});
        assert(HexDecode("0x") == std::vector<uint8_t>{});
        
        // Invalid hex
        assert(HexDecode("1g") == std::vector<uint8_t>{});
        assert(HexDecode("1a2") == std::vector<uint8_t>{}); // odd length
    }

    // 3. Glob matching
    {
        assert(GlobMatch("*.png", "image.png") == true);
        assert(GlobMatch("*.png", "image.jpg") == false);
        assert(GlobMatch("img?.png", "img1.png") == true);
        assert(GlobMatch("img?.png", "img12.png") == false);
        assert(GlobMatch("a*b?c", "axyzb1c") == true);
        assert(GlobMatch("a*b?c", "axyzbc") == false);
        assert(GlobMatch("*", "any_string_here") == true);
        assert(GlobMatch("abc", "abc") == true);
        assert(GlobMatch("abc", "abcd") == false);
        assert(GlobMatch("", "") == true);
        assert(GlobMatch("", "a") == false);
        assert(GlobMatch("a*", "a") == true);
    }

    // 4. Split/Join/Trim
    {
        std::string to_split = "one,two,,three";
        auto tokens = Split(to_split, ',');
        assert(tokens.size() == 4);
        assert(tokens[0] == "one");
        assert(tokens[1] == "two");
        assert(tokens[2] == "");
        assert(tokens[3] == "three");

        auto joined = Join(tokens, " - ");
        assert(joined == "one - two -  - three");

        std::string uncleaned = "  \t  hello world  \r\n ";
        assert(Trim(uncleaned) == "hello world");
        
        assert(Trim("   ") == "");
        assert(Trim("") == "");
    }

    std::cout << "StringUtils tests passed!\n";
}

void TestConfigParser() {
    std::cout << "Running extended ConfigParser tests...\n";

    std::string ini_content = 
        "# Global Configuration\n"
        "[Global]\n"
        "threads = 8\n"
        "ratio = 1.5\n"
        "verbose = true\n"
        "comment_test = hello ; inline comment\n"
        "escaped = \"Line1\\nLine2\\tTabbed\"\n"
        "\n"
        "[Batch]\n"
        "input_pattern = *.png\n"
        "multiline_val = Part 1 \\\n"
        "Part 2 \\\n"
        "Part 3\n"
        "\n"
        "[EmptySection]\n"
        "\n"
        "[EdgeCases]\n"
        "empty_val = \n"
        "spaces =   some space-padded value   \n"
        "quoted = \"value in quotes\"\n";

    ConfigParser parser;
    bool success = parser.LoadFromString(ini_content);
    assert(success);

    assert(parser.HasSection("Global"));
    assert(parser.HasSection("Batch"));
    assert(parser.HasSection("EmptySection"));
    assert(parser.HasSection("EdgeCases"));
    assert(!parser.HasSection("Missing"));

    // Global
    assert(parser.GetInt("Global", "threads") == 8);
    assert(parser.GetDouble("Global", "ratio") == 1.5);
    assert(parser.GetBool("Global", "verbose") == true);
    assert(parser.GetString("Global", "comment_test") == "hello");
    assert(parser.GetString("Global", "escaped") == "Line1\nLine2\tTabbed");

    // Batch
    assert(parser.GetString("Batch", "input_pattern") == "*.png");
    assert(parser.GetString("Batch", "multiline_val") == "Part 1Part 2Part 3");

    // EdgeCases
    assert(parser.GetString("EdgeCases", "empty_val") == "");
    assert(parser.GetString("EdgeCases", "spaces") == "some space-padded value");
    assert(parser.GetString("EdgeCases", "quoted") == "value in quotes");

    // Default values
    assert(parser.GetString("Global", "nonexistent", "def") == "def");
    assert(parser.GetInt("Global", "nonexistent", 42) == 42);
    assert(parser.GetDouble("Global", "nonexistent", 3.14) == 3.14);
    assert(parser.GetBool("Global", "nonexistent", false) == false);

    // Save and Reload
    std::string saved = parser.SaveToString();
    ConfigParser parser2;
    assert(parser2.LoadFromString(saved));
    assert(parser2.GetInt("Global", "threads") == 8);
    assert(parser2.GetString("EdgeCases", "quoted") == "value in quotes");

    std::cout << "ConfigParser tests passed!\n";
}

void TestThreadPool() {
    std::cout << "Running extended ThreadPool tests...\n";

    ThreadPool pool(4);
    assert(pool.GetWorkerCount() == 4);

    // Submit priority tasks
    std::vector<std::future<int>> futures;
    std::vector<int> execution_order;
    std::mutex order_mutex;

    auto make_task = [&](int id, int priority) {
        return pool.Submit(priority, [id, &execution_order, &order_mutex]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(id);
            return id * 10;
        });
    };

    // Saturate the pool first
    std::vector<std::future<int>> saturating_tasks;
    for (int i = 0; i < 4; ++i) {
        saturating_tasks.push_back(pool.Submit(0, []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 0;
        }));
    }

    // Submit tasks with different priorities while workers are busy
    auto f_low = make_task(1, 10);
    auto f_high = make_task(2, 100);
    auto f_med = make_task(3, 50);

    // Wait for saturating tasks
    for (auto& f : saturating_tasks) f.get();
    
    int val_high = f_high.get();
    int val_med = f_med.get();
    int val_low = f_low.get();

    assert(val_high == 20);
    assert(val_med == 30);
    assert(val_low == 10);

    // Verify priority order (should execute: high (2), then med (3), then low (1))
    {
        std::lock_guard<std::mutex> lock(order_mutex);
        assert(execution_order.size() == 3);
        assert(execution_order[0] == 2); // high priority (100)
        assert(execution_order[1] == 3); // medium priority (50)
        assert(execution_order[2] == 1); // low priority (10)
    }

    // Dynamic resize
    pool.Resize(2);
    assert(pool.GetWorkerCount() == 2);

    pool.Resize(6);
    assert(pool.GetWorkerCount() == 6);

    // Exception handling in tasks
    auto f_exc = pool.Submit(10, []() {
        throw std::runtime_error("Task failed exception");
        return 0;
    });

    try {
        f_exc.get();
        assert(false); // should throw
    } catch (const std::runtime_error& e) {
        assert(std::string(e.what()) == "Task failed exception");
    }

    std::cout << "ThreadPool tests passed!\n";
}

void TestMetadata() {
    std::cout << "Running Metadata tests...\n";

    // 1. Comments Block
    auto* cb = new CommentsBlock();
    cb->type = MetadataType::COMMENTS;
    cb->author = "DeepMind Agent";
    cb->comment = "PixelForge Metadata Block Serialization Testing";
    cb->timestamp = 1719400000;

    // 2. EXIF Block
    auto* eb = new EXIFBlock();
    eb->type = MetadataType::EXIF;
    eb->camera_model = "Hasselblad X2D";
    eb->exposure_time = 0.008f; // 1/125s
    eb->f_number = 2.8f;
    eb->iso_speed = 100;
    eb->thumbnail_size = 8;
    eb->raw_thumbnail = new uint8_t[8]{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

    std::vector<const MetadataBlock*> blocks = {cb, eb};

    // Serialize
    std::vector<uint8_t> serialized = serialize_metadata(blocks);
    assert(!serialized.empty());

    // Deserialize
    std::vector<MetadataBlock*> parsed = parse_metadata(serialized.data(), serialized.size());
    assert(parsed.size() == 2);

    // Verify Comments Block
    assert(parsed[0]->type == MetadataType::COMMENTS);
    auto* parsed_cb = static_cast<CommentsBlock*>(parsed[0]);
    assert(parsed_cb->author == cb->author);
    assert(parsed_cb->comment == cb->comment);
    assert(parsed_cb->timestamp == cb->timestamp);

    // Verify EXIF Block
    assert(parsed[1]->type == MetadataType::EXIF);
    auto* parsed_eb = static_cast<EXIFBlock*>(parsed[1]);
    assert(parsed_eb->camera_model == eb->camera_model);
    assert(parsed_eb->exposure_time == eb->exposure_time);
    assert(parsed_eb->f_number == eb->f_number);
    assert(parsed_eb->iso_speed == eb->iso_speed);
    assert(parsed_eb->thumbnail_size == eb->thumbnail_size);
    assert(std::memcmp(parsed_eb->raw_thumbnail, eb->raw_thumbnail, 8) == 0);

    // Type-safe metadata accessors reject mismatched block types.
    EXIFBlock* confused_exif = get_exif_block(parsed[0]);
    assert(confused_exif == nullptr);

    // Clean up
    for (auto* p : parsed) delete p;
    delete cb;
    delete eb;

    std::cout << "Metadata tests passed!\n";
}

int main() {
    try {
        TestStringUtils();
        TestConfigParser();
        TestThreadPool();
        TestMetadata();
        std::cout << "All utility tests passed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
