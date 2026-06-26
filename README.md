# FenrerVFS (Virtual File System & Binary Archive Decoder)

`FenrerVFS` is a lightweight, header-only-capable C++ systems programming library designed for parsing and mounting custom structured binary archive files (`.fnfs` format) into an in-memory Virtual File System (VFS). 

The library supports directory hierarchies, stateful file descriptor operations, custom Run-Length Encoding (RLE) decompression, and an active Least-Recently Used (LRU) file cache system.

## Features
* **Binary Archive Decoding**: Reads a custom header and directory table structure containing metadata, compressed/uncompressed sizes, and offsets.
* **In-Memory VFS**: Dynamically mounts directories and file nodes into an interactive tree structure.
* **Stateful File Operations**: Simulates file descriptors (`open`, `read`, `delete`) on virtual file structures.
* **LRU Cache**: Automatically caches and evicts recently accessed file contents to optimize read operations.
* **Custom Decompression**: Built-in Run-Length Encoding (RLE) decompression engine for packed file entries.

## Project Structure
* `src/`: Core header and implementation source files (`vfs.h`, `vfs.cc`).
* `fuzz/`: Fuzzing harnesses targeting different library entry points (`mount`, `extraction`, `vfs state`).
* `.clusterfuzzlite/`: Configuration and build scripts for ClusterFuzzLite integration.
* `tests/`: Test generator and verification assets.
