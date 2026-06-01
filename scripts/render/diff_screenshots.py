#!/usr/bin/env python3
"""Compare two uncompressed BMP screenshots and optionally write a BMP diff."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


class BmpImage:
    def __init__(self, width: int, height: int, pixels: bytes) -> None:
        self.width = width
        self.height = height
        self.pixels = pixels


def read_bmp(path: Path) -> BmpImage:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError(f"{path} is not a BMP file")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise ValueError(f"{path} has unsupported BMP DIB header size {dib_size}")

    width = struct.unpack_from("<i", data, 18)[0]
    raw_height = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or compression != 0 or bpp not in (24, 32):
        raise ValueError(f"{path} must be uncompressed 24-bit or 32-bit BMP")
    if width <= 0 or raw_height == 0:
        raise ValueError(f"{path} has invalid dimensions {width}x{raw_height}")

    height = abs(raw_height)
    bytes_per_pixel = bpp // 8
    stride = ((width * bytes_per_pixel + 3) // 4) * 4
    rows = []
    top_down = raw_height < 0
    for y in range(height):
        source_y = y if top_down else height - 1 - y
        start = pixel_offset + source_y * stride
        row = data[start:start + width * bytes_per_pixel]
        rgb = bytearray(width * 3)
        for x in range(width):
            src = x * bytes_per_pixel
            dst = x * 3
            rgb[dst + 0] = row[src + 2]
            rgb[dst + 1] = row[src + 1]
            rgb[dst + 2] = row[src + 0]
        rows.append(bytes(rgb))
    return BmpImage(width, height, b"".join(rows))


def write_bmp(path: Path, image: BmpImage) -> None:
    row_bytes = image.width * 3
    stride = ((row_bytes + 3) // 4) * 4
    padding = b"\0" * (stride - row_bytes)
    pixel_size = stride * image.height
    file_size = 14 + 40 + pixel_size

    out = bytearray()
    out += struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54)
    out += struct.pack("<IiiHHIIiiII", 40, image.width, image.height, 1, 24,
                       0, pixel_size, 2835, 2835, 0, 0)
    for y in range(image.height - 1, -1, -1):
        start = y * row_bytes
        row = image.pixels[start:start + row_bytes]
        bgr = bytearray(row_bytes)
        for x in range(image.width):
            src = x * 3
            bgr[src + 0] = row[src + 2]
            bgr[src + 1] = row[src + 1]
            bgr[src + 2] = row[src + 0]
        out += bgr
        out += padding
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(out)


def compare(reference: BmpImage, actual: BmpImage, threshold: int) -> tuple[dict[str, float | int], BmpImage]:
    if reference.width != actual.width or reference.height != actual.height:
        raise ValueError(
            f"dimension mismatch: reference {reference.width}x{reference.height}, "
            f"actual {actual.width}x{actual.height}"
        )

    diff_pixels = bytearray(len(reference.pixels))
    differing_pixels = 0
    total_delta = 0
    max_delta = 0
    pixel_count = reference.width * reference.height

    for i in range(0, len(reference.pixels), 3):
        dr = abs(reference.pixels[i + 0] - actual.pixels[i + 0])
        dg = abs(reference.pixels[i + 1] - actual.pixels[i + 1])
        db = abs(reference.pixels[i + 2] - actual.pixels[i + 2])
        delta = max(dr, dg, db)
        total_delta += dr + dg + db
        max_delta = max(max_delta, delta)
        if delta > threshold:
            differing_pixels += 1
            diff_pixels[i + 0] = 255
            diff_pixels[i + 1] = min(255, delta * 4)
            diff_pixels[i + 2] = 0
        else:
            gray = (reference.pixels[i + 0] + reference.pixels[i + 1] + reference.pixels[i + 2]) // 12
            diff_pixels[i + 0] = gray
            diff_pixels[i + 1] = gray
            diff_pixels[i + 2] = gray

    summary = {
        "width": reference.width,
        "height": reference.height,
        "threshold": threshold,
        "differing_pixels": differing_pixels,
        "differing_percent": (differing_pixels * 100.0) / pixel_count,
        "max_channel_delta": max_delta,
        "average_channel_delta": total_delta / len(reference.pixels),
    }
    return summary, BmpImage(reference.width, reference.height, bytes(diff_pixels))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("actual", type=Path)
    parser.add_argument("--threshold", type=int, default=8)
    parser.add_argument("--diff-out", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    reference = read_bmp(args.reference)
    actual = read_bmp(args.actual)
    summary, diff = compare(reference, actual, args.threshold)

    if args.diff_out is not None:
        write_bmp(args.diff_out, diff)
        summary["diff_path"] = str(args.diff_out)

    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(
            "dimensions={width}x{height} threshold={threshold} "
            "differing={differing_pixels} ({differing_percent:.4f}%) "
            "max_delta={max_channel_delta} avg_delta={average_channel_delta:.4f}".format(**summary)
        )
        if "diff_path" in summary:
            print(f"diff={summary['diff_path']}")

    return 1 if summary["differing_pixels"] > 0 else 0


if __name__ == "__main__":
    raise SystemExit(main())
