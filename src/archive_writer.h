/**
 * @file archive_writer.h
 * @brief Writer for FNFS (.fnfs) archive files.
 *
 * ArchiveWriter builds an FNFS archive in memory from individual files and
 * directories.  The resulting byte buffer can be written to disk or passed
 * directly to VFS::mount_archive() for round-trip testing.
 *
 * Archive layout produced by finalize():
 *   ┌──────────────────────┐  offset 0
 *   │   ArchiveHeader      │  12 bytes (magic + num_files + dir_offset)
 *   ├──────────────────────┤
 *   │   File data blob     │  concatenated (optionally compressed) payloads
 *   ├──────────────────────┤  <- dir_offset
 *   │   DirectoryEntry[]   │  num_files × sizeof(DirectoryEntry)
 *   └──────────────────────┘
 *
 * Supported compression modes per-file:
 *   NONE    – raw bytes (no transformation)
 *   RLE     – run-length encoding (matches RLEDecompressor)
 *   HUFFMAN – Huffman coding    (matches HuffmanDecompressor)
 *   LZW     – Lempel-Ziv-Welch (matches LZWDecompressor, 12-bit codes)
 *
 * Thread-safety: None. A single ArchiveWriter should only be used from one
 *                thread at a time.
 *
 * Copyright (c) 2026 Project Fenrer Contributors.
 */

#ifndef ARCHIVE_WRITER_H
#define ARCHIVE_WRITER_H

#include "vfs.h"       // ArchiveHeader, DirectoryEntry

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  CompressionType – mirrors the `compression` field in DirectoryEntry
// ---------------------------------------------------------------------------

/**
 * @enum CompressionType
 * @brief Identifies which compressor to apply when writing a file entry.
 *
 * The numeric values intentionally match the 1-byte `compression` field in
 * DirectoryEntry so no translation is needed at serialization time.
 */
enum class CompressionType : uint8_t {
    NONE    = 0,   ///< Store the file data as-is.
    RLE     = 1,   ///< Run-length encoding.
    HUFFMAN = 2,   ///< Huffman coding with 256-entry frequency table header.
    LZW     = 3    ///< LZW with 12-bit codes (max dictionary 4096 entries).
};

// ---------------------------------------------------------------------------
//  PendingEntry – internal bookkeeping for one file or directory
// ---------------------------------------------------------------------------

/**
 * @struct PendingEntry
 * @brief Metadata accumulated for each add_file() / add_directory() call
 *        before finalize() assembles the archive.
 */
struct PendingEntry {
    /// Virtual path inside the archive (e.g. "/textures/wall.png").
    std::string path;

    /// True for directories, false for regular files.
    bool is_directory;

    /// Compression algorithm applied to this entry's data.
    CompressionType compression;

    /// Original (uncompressed) size in bytes.
    uint32_t original_size;

    /// Compressed size in bytes (equals original_size when NONE).
    uint32_t compressed_size;

    /// Offset of this entry's data within the file-data blob.
    uint32_t data_offset;

    /// CRC-32 checksum of the *original* (uncompressed) data.
    uint32_t crc32;
};

// ---------------------------------------------------------------------------
//  ArchiveWriter
// ---------------------------------------------------------------------------

/**
 * @class ArchiveWriter
 * @brief Constructs an FNFS archive in memory.
 *
 * Usage:
 * @code
 *   ArchiveWriter w;
 *   w.begin_archive();
 *   w.set_compression_level(CompressionType::RLE);
 *   w.add_directory("/textures");
 *   w.add_file("/textures/wall.png", raw_pixels);
 *   std::vector<uint8_t> archive;
 *   if (w.finalize(archive)) {
 *       // archive now holds a valid FNFS image
 *   }
 * @endcode
 */
class ArchiveWriter {
public:
    // ---- Construction / destruction --------------------------------------

    ArchiveWriter();
    ~ArchiveWriter();

    // Non-copyable, movable.
    ArchiveWriter(const ArchiveWriter&) = delete;
    ArchiveWriter& operator=(const ArchiveWriter&) = delete;
    ArchiveWriter(ArchiveWriter&&) noexcept = default;
    ArchiveWriter& operator=(ArchiveWriter&&) noexcept = default;

    // ---- Archive lifecycle -----------------------------------------------

    /**
     * Begin a new archive. Resets all internal state (entries, data buffer,
     * default compression) so the writer can be reused.
     */
    void begin_archive();

    /**
     * Set the default compression algorithm for subsequent add_file() calls
     * that do not specify an override.
     * @param type  The compression algorithm to use.
     */
    void set_compression_level(CompressionType type);

    /**
     * Add a file to the archive.
     * @param path            Virtual path (e.g. "/docs/readme.txt").
     * @param data            Raw (uncompressed) file contents.
     * @param compression     Compression algorithm to use. If not specified,
     *                        the default set by set_compression_level() is
     *                        used.
     * @return true on success, false on error (e.g. path too long).
     */
    bool add_file(const std::string& path,
                  const std::vector<uint8_t>& data,
                  CompressionType compression);

    /** Overload that uses the current default compression. */
    bool add_file(const std::string& path,
                  const std::vector<uint8_t>& data);

    /**
     * Add a directory marker entry to the archive.
     * Directory entries carry no data but appear in the directory table
     * so that the VFS can reconstruct the directory tree on mount.
     * @param path  Virtual directory path (e.g. "/textures").
     * @return true on success.
     */
    bool add_directory(const std::string& path);

    /**
     * Assemble the final FNFS archive image.
     *
     * Layout:  ArchiveHeader | file-data-blob | DirectoryEntry[]
     *
     * @param output  Output buffer – replaced with the complete archive.
     * @return true on success.
     */
    bool finalize(std::vector<uint8_t>& output);

    // ---- Informational ---------------------------------------------------

    /** @return Number of entries (files + directories) queued so far. */
    size_t entry_count() const { return entries_.size(); }

    /** @return Current size of the compressed data blob (bytes). */
    size_t data_blob_size() const { return data_blob_.size(); }

    /** @return True after begin_archive() and before finalize(). */
    bool is_open() const { return archive_open_; }

private:
    // ---- Internal compression helpers ------------------------------------

    /**
     * Compress @p src using run-length encoding.
     * Format: pairs of (count, byte).  Runs longer than 255 are split
     * into multiple pairs.  Non-repeating bytes use count = 1.
     */
    static std::vector<uint8_t> compress_rle(const std::vector<uint8_t>& src);

    /**
     * Compress @p src using LZW with 12-bit codes (max dictionary 4096).
     * Produces the exact inverse of LZWDecompressor::decompress().
     */
    static std::vector<uint8_t> compress_lzw(const std::vector<uint8_t>& src);

    /**
     * Compress @p src using Huffman coding.
     * Output format:
     *   [1024 bytes] 256 × uint32 LE frequency table
     *   [variable]   packed bitstream (MSB-first within each byte)
     * This matches the format expected by HuffmanDecompressor::decompress().
     */
    static std::vector<uint8_t> compress_huffman(
        const std::vector<uint8_t>& src);

    /**
     * Truncate or pad @p name into a 32-byte buffer suitable for
     * DirectoryEntry::filename. Always null-terminates.
     */
    static void fill_filename(char (&dest)[32], const std::string& name);

    // ---- State -----------------------------------------------------------

    /// True between begin_archive() and finalize().
    bool archive_open_;

    /// Default compression for add_file() calls without an explicit override.
    CompressionType default_compression_;

    /// Accumulated compressed file data (concatenated, no padding).
    std::vector<uint8_t> data_blob_;

    /// Metadata for every entry added so far.
    std::vector<PendingEntry> entries_;
};

#endif // ARCHIVE_WRITER_H
