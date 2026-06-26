/**
 * @file archive_writer.cc
 * @brief Implementation of ArchiveWriter – builds FNFS archives in memory.
 *
 * This file implements:
 *   - begin_archive / finalize  (archive lifecycle)
 *   - add_file / add_directory  (entry accumulation)
 *   - compress_rle              (run-length encoding compressor)
 *   - compress_lzw              (LZW compressor, 12-bit codes)
 *   - compress_huffman          (Huffman coding compressor)
 *   - set_compression_level     (default compression config)
 *
 * The compressors produce byte streams that are exactly decompressible by
 * the corresponding VFSDecompressor subclasses (RLE, LZW, Huffman).
 *
 * Copyright (c) 2026 Project Fenrer Contributors.
 */

#include "archive_writer.h"
#include "checksum.h"
#include "logger.h"
#include "path_utils.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <queue>
#include <sstream>
#include <unordered_map>

// ===================================================================
//  Construction / destruction
// ===================================================================

ArchiveWriter::ArchiveWriter()
    : archive_open_(false),
      default_compression_(CompressionType::NONE)
{
    VFSLogger::get_instance().debug("ArchiveWriter",
                                    "ArchiveWriter instance created");
}

ArchiveWriter::~ArchiveWriter()
{
    if (archive_open_) {
        VFSLogger::get_instance().warn(
            "ArchiveWriter",
            "Destroyed with an open archive that was never finalized");
    }
}

// ===================================================================
//  Archive lifecycle
// ===================================================================

void ArchiveWriter::begin_archive()
{
    entries_.clear();
    data_blob_.clear();
    default_compression_ = CompressionType::NONE;
    archive_open_ = true;

    VFSLogger::get_instance().info("ArchiveWriter",
                                   "New archive started – state reset");
}

void ArchiveWriter::set_compression_level(CompressionType type)
{
    default_compression_ = type;

    const char* name = "NONE";
    switch (type) {
        case CompressionType::NONE:    name = "NONE";    break;
        case CompressionType::RLE:     name = "RLE";     break;
        case CompressionType::HUFFMAN: name = "HUFFMAN"; break;
        case CompressionType::LZW:    name = "LZW";     break;
    }
    VFSLogger::get_instance().info(
        "ArchiveWriter",
        std::string("Default compression set to ") + name);
}

// ===================================================================
//  add_file
// ===================================================================

bool ArchiveWriter::add_file(const std::string& path,
                             const std::vector<uint8_t>& data)
{
    return add_file(path, data, default_compression_);
}

bool ArchiveWriter::add_file(const std::string& path,
                             const std::vector<uint8_t>& data,
                             CompressionType compression)
{
    if (!archive_open_) {
        VFSLogger::get_instance().error(
            "ArchiveWriter",
            "add_file called but no archive is open – call begin_archive()");
        return false;
    }

    // Validate the filename component – must fit in 31 chars + NUL.
    std::string filename = VFSPathUtils::get_filename(path);
    if (filename.empty()) {
        VFSLogger::get_instance().error(
            "ArchiveWriter",
            "Cannot add file with empty filename: \"" + path + "\"");
        return false;
    }
    if (filename.size() > 31) {
        VFSLogger::get_instance().warn(
            "ArchiveWriter",
            "Filename \"" + filename +
                "\" exceeds 31 chars – will be truncated in directory entry");
    }

    // Compute CRC-32 of the original data *before* compression.
    uint32_t crc = VFSChecksum::compute_crc32(data.data(), data.size());

    VFSLogger::get_instance().debug(
        "ArchiveWriter",
        "Adding file \"" + path + "\" (" + std::to_string(data.size()) +
            " bytes, CRC32=0x" +
            ([&]() {
                std::ostringstream h;
                h << std::hex << crc;
                return h.str();
            })() +
            ")");

    // --- Compress the data ------------------------------------------------
    std::vector<uint8_t> compressed;

    switch (compression) {
        case CompressionType::NONE:
            compressed = data;  // no transformation
            break;

        case CompressionType::RLE:
            compressed = compress_rle(data);
            VFSLogger::get_instance().debug(
                "ArchiveWriter",
                "RLE: " + std::to_string(data.size()) + " -> " +
                    std::to_string(compressed.size()) + " bytes");
            break;

        case CompressionType::HUFFMAN:
            compressed = compress_huffman(data);
            VFSLogger::get_instance().debug(
                "ArchiveWriter",
                "Huffman: " + std::to_string(data.size()) + " -> " +
                    std::to_string(compressed.size()) + " bytes");
            break;

        case CompressionType::LZW:
            compressed = compress_lzw(data);
            VFSLogger::get_instance().debug(
                "ArchiveWriter",
                "LZW: " + std::to_string(data.size()) + " -> " +
                    std::to_string(compressed.size()) + " bytes");
            break;
    }

    // If the compressor expanded the data (negative compression), fall back
    // to storing uncompressed to avoid wasting space.
    CompressionType actual = compression;
    if (compression != CompressionType::NONE &&
        compressed.size() >= data.size()) {
        VFSLogger::get_instance().info(
            "ArchiveWriter",
            "Compression expanded data for \"" + path +
                "\" – falling back to NONE");
        compressed = data;
        actual = CompressionType::NONE;
    }

    // --- Build the PendingEntry -------------------------------------------
    PendingEntry entry;
    entry.path           = path;
    entry.is_directory   = false;
    entry.compression    = actual;
    entry.original_size  = static_cast<uint32_t>(data.size());
    entry.compressed_size = static_cast<uint32_t>(compressed.size());
    entry.data_offset    = static_cast<uint32_t>(data_blob_.size());
    entry.crc32          = crc;

    // Append compressed payload to the data blob.
    data_blob_.insert(data_blob_.end(), compressed.begin(), compressed.end());
    entries_.push_back(std::move(entry));

    VFSLogger::get_instance().info(
        "ArchiveWriter",
        "File \"" + path + "\" added (entry #" +
            std::to_string(entries_.size()) + ")");
    return true;
}

// ===================================================================
//  add_directory
// ===================================================================

bool ArchiveWriter::add_directory(const std::string& path)
{
    if (!archive_open_) {
        VFSLogger::get_instance().error(
            "ArchiveWriter",
            "add_directory called but no archive is open");
        return false;
    }

    std::string dirname = VFSPathUtils::get_filename(path);
    if (dirname.empty()) {
        // Root directory "/" has no basename – that's fine, we store "/".
        dirname = "/";
    }

    PendingEntry entry;
    entry.path           = path;
    entry.is_directory   = true;
    entry.compression    = CompressionType::NONE;
    entry.original_size  = 0;
    entry.compressed_size = 0;
    entry.data_offset    = 0;
    entry.crc32          = 0;

    entries_.push_back(std::move(entry));

    VFSLogger::get_instance().info(
        "ArchiveWriter",
        "Directory \"" + path + "\" added (entry #" +
            std::to_string(entries_.size()) + ")");
    return true;
}

// ===================================================================
//  finalize
// ===================================================================

bool ArchiveWriter::finalize(std::vector<uint8_t>& output)
{
    if (!archive_open_) {
        VFSLogger::get_instance().error(
            "ArchiveWriter",
            "finalize called but no archive is open");
        return false;
    }

    VFSLogger::get_instance().info(
        "ArchiveWriter",
        "Finalizing archive: " + std::to_string(entries_.size()) +
            " entries, " + std::to_string(data_blob_.size()) +
            " bytes of file data");

    // Count only file entries (directories are markers with no data).
    uint32_t file_count = 0;
    for (auto& e : entries_) {
        if (!e.is_directory) ++file_count;
    }

    // --- Build the ArchiveHeader ------------------------------------------
    ArchiveHeader header;
    std::memset(&header, 0, sizeof(header));
    header.magic[0] = 'F';
    header.magic[1] = 'N';
    header.magic[2] = 'F';
    header.magic[3] = 'S';
    header.num_files  = static_cast<uint32_t>(entries_.size());

    // dir_offset = sizeof(header) + data_blob size.
    // The data offsets stored in each PendingEntry are relative to the
    // start of the data blob; we need to adjust them to be relative to
    // the start of the archive image (add sizeof(ArchiveHeader)).
    uint32_t header_size = static_cast<uint32_t>(sizeof(ArchiveHeader));
    header.dir_offset = header_size +
                        static_cast<uint32_t>(data_blob_.size());

    VFSLogger::get_instance().debug(
        "ArchiveWriter",
        "Header: num_files=" + std::to_string(header.num_files) +
            " dir_offset=" + std::to_string(header.dir_offset));

    // --- Build the DirectoryEntry table -----------------------------------
    std::vector<DirectoryEntry> dir_table;
    dir_table.reserve(entries_.size());

    for (auto& pe : entries_) {
        DirectoryEntry de;
        std::memset(&de, 0, sizeof(de));

        // Fill filename – use the full path so the parser can reconstruct
        // the tree.  Truncate to 31 chars + NUL.
        fill_filename(de.filename, pe.path);

        de.size          = pe.compressed_size;
        de.original_size = pe.original_size;
        // Adjust offset: make it absolute within the archive image.
        de.offset        = header_size + pe.data_offset;
        de.compression   = static_cast<uint8_t>(pe.compression);
        de.checksum      = pe.crc32;

        dir_table.push_back(de);
    }

    // --- Assemble the final buffer ----------------------------------------
    size_t total_size = sizeof(ArchiveHeader) +
                        data_blob_.size() +
                        dir_table.size() * sizeof(DirectoryEntry);

    output.clear();
    output.reserve(total_size);

    // 1) Header
    const auto* hdr_bytes = reinterpret_cast<const uint8_t*>(&header);
    output.insert(output.end(), hdr_bytes, hdr_bytes + sizeof(header));

    // 2) File data blob
    output.insert(output.end(), data_blob_.begin(), data_blob_.end());

    // 3) Directory table
    for (auto& de : dir_table) {
        const auto* de_bytes = reinterpret_cast<const uint8_t*>(&de);
        output.insert(output.end(), de_bytes, de_bytes + sizeof(de));
    }

    archive_open_ = false;

    VFSLogger::get_instance().info(
        "ArchiveWriter",
        "Archive finalized: " + std::to_string(total_size) +
            " bytes total (" + std::to_string(entries_.size()) +
            " entries, " + std::to_string(file_count) + " files)");
    return true;
}

// ===================================================================
//  fill_filename helper
// ===================================================================

void ArchiveWriter::fill_filename(char (&dest)[32], const std::string& name)
{
    std::memset(dest, 0, 32);
    size_t copy_len = std::min(name.size(), static_cast<size_t>(31));
    std::memcpy(dest, name.data(), copy_len);
    dest[copy_len] = '\0';
}

// ===================================================================
//  compress_rle  –  run-length encoding compressor
// ===================================================================
//
// Format: sequence of (count, byte) pairs.
//   - count is 1..255; runs > 255 emit multiple pairs.
//   - Non-repeating bytes get count = 1.
// This is the exact inverse of RLEDecompressor::decompress().

/*static*/
std::vector<uint8_t> ArchiveWriter::compress_rle(
    const std::vector<uint8_t>& src)
{
    VFSLogger::get_instance().debug("ArchiveWriter",
                                    "RLE compressing " +
                                        std::to_string(src.size()) +
                                        " bytes");

    std::vector<uint8_t> out;
    if (src.empty()) return out;

    // Reserve a rough estimate – worst case is 2× input (all unique bytes).
    out.reserve(src.size() * 2);

    size_t i = 0;
    while (i < src.size()) {
        uint8_t current = src[i];
        size_t run_start = i;

        // Count consecutive identical bytes (up to 255).
        while (i < src.size() && src[i] == current &&
               (i - run_start) < 255) {
            ++i;
        }

        uint8_t count = static_cast<uint8_t>(i - run_start);
        out.push_back(count);
        out.push_back(current);
    }

    VFSLogger::get_instance().debug(
        "ArchiveWriter",
        "RLE: input=" + std::to_string(src.size()) +
            " output=" + std::to_string(out.size()));
    return out;
}

// ===================================================================
//  compress_lzw  –  LZW compressor (12-bit codes, max dict 4096)
// ===================================================================
//
// Produces a bitstream of 12-bit codes (MSB-first per byte) that
// LZWDecompressor::decompress() can reconstruct.

/*static*/
std::vector<uint8_t> ArchiveWriter::compress_lzw(
    const std::vector<uint8_t>& src)
{
    VFSLogger::get_instance().debug("ArchiveWriter",
                                    "LZW compressing " +
                                        std::to_string(src.size()) +
                                        " bytes");

    if (src.empty()) return {};

    // Build the initial dictionary mapping single bytes to codes 0..255.
    std::map<std::vector<uint8_t>, uint32_t> dictionary;
    for (int i = 0; i < 256; ++i) {
        dictionary[{static_cast<uint8_t>(i)}] = static_cast<uint32_t>(i);
    }
    uint32_t dict_size = 256;
    const uint32_t MAX_DICT_SIZE = 4096;  // 12-bit codes

    // Accumulate 12-bit codes.
    std::vector<uint32_t> codes;
    codes.reserve(src.size());

    std::vector<uint8_t> w;
    w.push_back(src[0]);

    for (size_t i = 1; i < src.size(); ++i) {
        std::vector<uint8_t> wc = w;
        wc.push_back(src[i]);

        if (dictionary.count(wc)) {
            w = std::move(wc);
        } else {
            // Output the code for w.
            codes.push_back(dictionary[w]);

            // Add wc to the dictionary if there is space.
            if (dict_size < MAX_DICT_SIZE) {
                dictionary[wc] = dict_size++;
            }

            // Reset w to the current byte.
            w.clear();
            w.push_back(src[i]);
        }
    }

    // Output the code for whatever is left in w.
    if (!w.empty()) {
        codes.push_back(dictionary[w]);
    }

    // Pack 12-bit codes into a byte stream (MSB-first within each byte),
    // matching the bit layout that LZWDecompressor reads.
    size_t total_bits = codes.size() * 12;
    size_t total_bytes = (total_bits + 7) / 8;
    std::vector<uint8_t> out(total_bytes, 0);

    size_t bit_idx = 0;
    for (uint32_t code : codes) {
        // Write 12 bits, MSB first.
        for (int b = 11; b >= 0; --b) {
            bool bit = (code >> b) & 1;
            if (bit) {
                size_t byte_pos = bit_idx / 8;
                size_t bit_off  = 7 - (bit_idx % 8);
                out[byte_pos] |= static_cast<uint8_t>(1u << bit_off);
            }
            ++bit_idx;
        }
    }

    VFSLogger::get_instance().debug(
        "ArchiveWriter",
        "LZW: input=" + std::to_string(src.size()) +
            " codes=" + std::to_string(codes.size()) +
            " output=" + std::to_string(out.size()) +
            " dict_size=" + std::to_string(dict_size));
    return out;
}

// ===================================================================
//  compress_huffman  –  Huffman coding compressor
// ===================================================================
//
// Output format (must match HuffmanDecompressor expectations):
//   [1024 bytes]  256 × uint32_t LE frequency table
//   [variable]    packed bitstream (MSB-first within each byte)
//
// The decompressor rebuilds the Huffman tree from the frequency table
// and walks the bitstream to recover each symbol.

/*static*/
std::vector<uint8_t> ArchiveWriter::compress_huffman(
    const std::vector<uint8_t>& src)
{
    VFSLogger::get_instance().debug("ArchiveWriter",
                                    "Huffman compressing " +
                                        std::to_string(src.size()) +
                                        " bytes");

    if (src.empty()) return {};

    // --- Step 1: Compute byte frequencies ---------------------------------
    std::vector<uint32_t> freq(256, 0);
    for (uint8_t b : src) {
        ++freq[b];
    }

    // --- Step 2: Build the Huffman tree -----------------------------------
    // Minimal tree node – we only need it during code generation.
    struct HNode {
        int symbol;         // -1 for internal nodes
        uint32_t weight;
        HNode* left;
        HNode* right;
    };

    // We use a flat arena to avoid scattered allocations.
    std::vector<std::unique_ptr<HNode>> arena;

    auto make_node = [&](int sym, uint32_t w,
                         HNode* l, HNode* r) -> HNode* {
        auto n = std::make_unique<HNode>();
        n->symbol = sym;
        n->weight = w;
        n->left   = l;
        n->right  = r;
        HNode* raw = n.get();
        arena.push_back(std::move(n));
        return raw;
    };

    // Priority queue: smallest weight first.
    auto cmp = [](HNode* a, HNode* b) { return a->weight > b->weight; };
    std::priority_queue<HNode*, std::vector<HNode*>, decltype(cmp)> pq(cmp);

    for (int i = 0; i < 256; ++i) {
        if (freq[static_cast<size_t>(i)] > 0) {
            pq.push(make_node(i, freq[static_cast<size_t>(i)],
                              nullptr, nullptr));
        }
    }

    // Edge case: only one distinct symbol.
    if (pq.size() == 1) {
        HNode* only = pq.top();
        pq.pop();
        HNode* root = make_node(-1, only->weight, only, nullptr);
        pq.push(root);
    }

    while (pq.size() > 1) {
        HNode* left  = pq.top(); pq.pop();
        HNode* right = pq.top(); pq.pop();
        pq.push(make_node(-1, left->weight + right->weight, left, right));
    }

    HNode* tree_root = pq.empty() ? nullptr : pq.top();

    // --- Step 3: Generate code table via DFS ------------------------------
    // code_table[symbol] = vector of bools (MSB first path from root).
    std::vector<std::vector<bool>> code_table(256);

    std::function<void(HNode*, std::vector<bool>&)> gen_codes =
        [&](HNode* node, std::vector<bool>& prefix) {
            if (!node) return;
            if (node->symbol >= 0) {
                // Leaf – assign current prefix as the code.
                if (prefix.empty()) {
                    // Degenerate tree with a single symbol: use code '0'.
                    prefix.push_back(false);
                }
                code_table[static_cast<size_t>(node->symbol)] = prefix;
                return;
            }
            // Left child = 0, right child = 1  (matches decompressor).
            prefix.push_back(false);
            gen_codes(node->left, prefix);
            prefix.pop_back();

            prefix.push_back(true);
            gen_codes(node->right, prefix);
            prefix.pop_back();
        };

    std::vector<bool> prefix;
    gen_codes(tree_root, prefix);

    // --- Step 4: Encode the bitstream -------------------------------------
    // First, calculate total bits so we can allocate the output.
    size_t total_bits = 0;
    for (uint8_t b : src) {
        total_bits += code_table[b].size();
    }

    size_t bitstream_bytes = (total_bits + 7) / 8;
    std::vector<uint8_t> bitstream(bitstream_bytes, 0);

    size_t bit_idx = 0;
    for (uint8_t b : src) {
        for (bool bit : code_table[b]) {
            if (bit) {
                size_t byte_pos = bit_idx / 8;
                size_t bit_off  = 7 - (bit_idx % 8);
                bitstream[byte_pos] |= static_cast<uint8_t>(1u << bit_off);
            }
            ++bit_idx;
        }
    }

    // --- Step 5: Assemble output = frequency table + bitstream ------------
    std::vector<uint8_t> out;
    out.reserve(1024 + bitstream.size());

    // Write the 256-entry frequency table as little-endian uint32s.
    for (int i = 0; i < 256; ++i) {
        uint32_t f = freq[static_cast<size_t>(i)];
        out.push_back(static_cast<uint8_t>(f & 0xFF));
        out.push_back(static_cast<uint8_t>((f >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((f >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((f >> 24) & 0xFF));
    }

    out.insert(out.end(), bitstream.begin(), bitstream.end());

    VFSLogger::get_instance().debug(
        "ArchiveWriter",
        "Huffman: input=" + std::to_string(src.size()) +
            " total_bits=" + std::to_string(total_bits) +
            " output=" + std::to_string(out.size()) +
            " (1024 header + " + std::to_string(bitstream.size()) +
            " bitstream)");
    return out;
}
