"""Generate deterministic, non-crashing seeds for PixelForge fuzz targets."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "fuzz" / "corpus"


def write(relative: str, data: bytes) -> None:
    destination = CORPUS / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def bmp_2x2() -> bytes:
    width, height, channels = 2, 2, 3
    row_stride = 8
    pixels = bytes(
        [0, 0, 255, 0, 255, 0, 0, 0,
         255, 0, 0, 255, 255, 255, 0, 0]
    )
    file_size = 54 + len(pixels)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54)
    dib = struct.pack(
        "<IiiHHIIiiII", 40, width, height, 1, channels * 8, 0,
        len(pixels), 2835, 2835, 0, 0
    )
    return header + dib + pixels


def tga_2x2() -> bytes:
    header = struct.pack(
        "<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 24, 0x20
    )
    pixels = bytes([0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255])
    return header + pixels


def gif_1x1() -> bytes:
    return bytes.fromhex(
        "47494638396101000100800000"
        "000000ffffff"
        "2c000000000100010000"
        "0202440100"
        "3b"
    )


def main() -> None:
    write("fuzz_bmp/valid_2x2.bmp", bmp_2x2())
    write("fuzz_tga/valid_2x2.tga", tga_2x2())
    write("fuzz_gif/valid_1x1.gif", gif_1x1())
    write("fuzz_imgtool/basic_commands.txt", b"create 8 8 3\nstats\ngrayscale\nstats\n")
    write(
        "fuzz_imgtool/metadata_commands.txt",
        b'metadata add comment "PixelForge" "seed" 1\nmetadata view\n',
    )


if __name__ == "__main__":
    main()
