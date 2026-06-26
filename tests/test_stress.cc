#include "../src/thread_pool.h"
#include "../src/allocator.h"
#include "../src/image.h"
#include "../src/filter.h"
#include "../src/effects_advanced.h"
#include "../src/config_parser.h"
#include "../src/string_utils.h"
#include "../src/logger.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <future>
#include <cassert>
#include <atomic>
#include <thread>
#include <sstream>
#include <memory>
#include <cmath>

using namespace PixelForge;

void StressThreadPool() {
    std::cout << "Starting ThreadPool stress test (10,000 tasks)...\n";
    ThreadPool pool(8);
    std::atomic<int> completed_tasks{0};
    const int total_tasks = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;
    futures.reserve(total_tasks);

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> prio_dist(0, 100);

    for (int i = 0; i < total_tasks; ++i) {
        int priority = prio_dist(rng);
        futures.push_back(pool.Submit(priority, [&completed_tasks]() {
            // Simulate brief workload
            double val = 0.0;
            for (int k = 0; k < 50; ++k) {
                val += std::sin(k) * std::cos(k);
            }
            (void)val;
            completed_tasks++;
        }));
    }

    // Dynamically resize thread pool while running tasks
    std::thread resizer([&pool]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            int num_threads = 4 + (i % 2) * 8; // toggle between 4 and 12 threads
            pool.Resize(num_threads);
        }
    });

    // Wait for all tasks
    for (auto& f : futures) {
        f.get();
    }
    
    resizer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    assert(completed_tasks == total_tasks);
    std::cout << "ThreadPool stress test passed. Completed " << completed_tasks 
              << " tasks in " << elapsed.count() << " ms.\n";
}

void StressAllocator() {
    std::cout << "Starting FrameBufferAllocator stress test...\n";
    auto allocator = FrameBufferAllocator::create(4); // Pool capacity of 4

    const int iterations = 1000;
    std::mt19937 rng(54321);
    std::uniform_int_distribution<int> size_dist(16, 256);
    std::uniform_int_distribution<int> format_dist(0, 2);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        std::vector<std::shared_ptr<Image>> active_buffers;
        int num_allocs = 1 + (i % 4); // Allocate between 1 and 4 buffers

        for (int j = 0; j < num_allocs; ++j) {
            int w = size_dist(rng);
            int h = size_dist(rng);
            PixelFormat fmt = static_cast<PixelFormat>(format_dist(rng));
            
            auto img = allocator->acquire(w, h, fmt);
            assert(img != nullptr);
            assert(img->getWidth() == static_cast<uint32_t>(w));
            assert(img->getHeight() == static_cast<uint32_t>(h));
            active_buffers.push_back(img);
        }
        // Release buffers at end of block scope
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "Allocator stress test passed. Ran " << iterations 
              << " cycles of acquisitions in " << elapsed.count() << " ms.\n";
}

void StressConfigParser() {
    std::cout << "Starting ConfigParser large-scale stress test...\n";
    std::stringstream ss;
    
    // Generate a 1,000-line INI configuration
    for (int sec = 0; sec < 50; ++sec) {
        ss << "[Section_" << sec << "]\n";
        for (int key = 0; key < 20; ++key) {
            ss << "param_" << key << " = " << (sec * 100 + key) << "\n";
        }
        ss << "\n";
    }

    ConfigParser parser;
    auto start_time = std::chrono::high_resolution_clock::now();

    bool success = parser.LoadFromString(ss.str());
    assert(success);

    // Verify properties
    for (int sec = 0; sec < 50; ++sec) {
        std::string section_name = "Section_" + std::to_string(sec);
        assert(parser.HasSection(section_name));
        for (int key = 0; key < 20; ++key) {
            std::string param_name = "param_" + std::to_string(key);
            int expected_val = sec * 100 + key;
            assert(parser.GetInt(section_name, param_name) == expected_val);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "ConfigParser stress test passed. Loaded and verified 1,000 keys in " 
              << elapsed.count() << " ms.\n";
}

void StressStringCodecs() {
    std::cout << "Starting StringUtils stress test (1MB random payload)...\n";
    
    // Generate 1MB random payload
    std::vector<uint8_t> payload(1024 * 1024);
    std::mt19937 rng(98765);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(byte_dist(rng));
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Base64
    std::string b64_encoded = StringUtils::Base64Encode(payload);
    std::vector<uint8_t> b64_decoded = StringUtils::Base64Decode(b64_encoded);
    assert(payload == b64_decoded);

    // Hex
    std::string hex_encoded = StringUtils::HexEncode(payload, false);
    std::vector<uint8_t> hex_decoded = StringUtils::HexDecode(hex_encoded);
    assert(payload == hex_decoded);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "StringUtils stress test passed. Processed 1MB Base64/Hex in " 
              << elapsed.count() << " ms.\n";
}

void StressImagePipeline() {
    std::cout << "Starting Multithreaded Image Pipeline stress test (500 pipeline chains)...\n";
    ThreadPool pool(8);
    std::atomic<int> completed_pipelines{0};
    const int total_pipelines = 500;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::future<void>> futures;
    futures.reserve(total_pipelines);

    for (int i = 0; i < total_pipelines; ++i) {
        int id = i;
        futures.push_back(pool.Submit(50, [id, &completed_pipelines]() {
            // Step 1: Create a random canvas size
            uint32_t width = 32 + (id % 16) * 4;
            uint32_t height = 32 + (id % 16) * 4;
            Image src(width, height, PixelFormat::RGB888);
            
            // Fill with some pattern
            auto& data = src.getData();
            for (size_t k = 0; k < data.size(); ++k) {
                data[k] = static_cast<uint8_t>((k + id) * 17);
            }

            Image step1, step2, step3, step4;
            // Step 2: Apply a chain of AdvancedEffects filters
            PixelForgeErrorCode err = AdvancedEffects::Posterize(src, step1, 4 + (id % 6));
            if (err == PixelForgeErrorCode::SUCCESS) {
                err = AdvancedEffects::Solarize(step1, step2, 100 + (id % 50));
            }
            if (err == PixelForgeErrorCode::SUCCESS) {
                err = AdvancedEffects::Pixelate(step2, step3, 2 + (id % 4));
            }
            if (err == PixelForgeErrorCode::SUCCESS) {
                err = AdvancedEffects::FloydSteinbergDither(step3, step4, DitherPalette::EGA);
            }

            if (err == PixelForgeErrorCode::SUCCESS) {
                assert(step4.getWidth() == width);
                assert(step4.getHeight() == height);
                completed_pipelines++;
            }
        }));
    }

    // Wait for all pipelines
    for (auto& f : futures) {
        f.get();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    assert(completed_pipelines == total_pipelines);
    std::cout << "Image Pipeline stress test passed. Completed " << completed_pipelines 
              << " chains in " << elapsed.count() << " ms.\n";
}

void StressLargeMemoryAcquisition() {
    std::cout << "Starting Large Memory Buffer recycling stress test...\n";
    auto allocator = FrameBufferAllocator::create(2); // capacity of 2

    auto start_time = std::chrono::high_resolution_clock::now();

    // Repeatedly request, use, and discard large buffers (e.g. 4MB per frame)
    for (int i = 0; i < 150; ++i) {
        // Allocate a 1024x1024 RGBA image (4MB)
        auto img1 = allocator->acquire(1024, 1024, PixelFormat::RGBA8888);
        assert(img1 != nullptr);
        
        // Touch some memory
        auto& data1 = img1->getData();
        data1[0] = 255;
        data1[data1.size() - 1] = 128;

        // Allocate a second large buffer
        auto img2 = allocator->acquire(1024, 1024, PixelFormat::RGBA8888);
        assert(img2 != nullptr);
        auto& data2 = img2->getData();
        data2[0] = 128;
        data2[data2.size() - 1] = 255;

        // Release first and acquire third to trigger recycling
        img1.reset();
        auto img3 = allocator->acquire(1024, 1024, PixelFormat::RGBA8888);
        assert(img3 != nullptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "Large Memory recycling stress test passed in " << elapsed.count() << " ms.\n";
}

int main() {
    std::cout << "Starting PixelForge Stress Test Suite...\n\n";

    StressThreadPool();
    StressAllocator();
    StressConfigParser();
    StressStringCodecs();
    StressImagePipeline();
    StressLargeMemoryAcquisition();

    std::cout << "\nAll PixelForge Stress Tests Completed Successfully!\n";
    return 0;
}

