#!/usr/bin/env python3
"""Generates assets/AppIcon.icns - the app's Finder/Dock icon.

The icon reuses the calendar's own visual language: a pastel pink tile, the
rose circle used for a logged day, and the soft ring used for today.

Shapes are drawn from signed distance fields and antialiased analytically, so
each size is rendered sharp at its own resolution rather than downscaled from
one big image. Run this only when changing the icon:

    python3 tools/make_icon.py && <rebuild>
"""

import math
import os
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Matches the palette in src/ui.h
PINK_TOP    = (252, 233, 238)
PINK_BOTTOM = (242, 198, 212)
ROSE        = (180,  60, 100)
CREAM       = (255, 252, 250)


def smoothstep(edge, dist):
    """Coverage of a pixel given its signed distance to a shape edge."""
    t = (edge - dist) / 1.0 + 0.5
    return max(0.0, min(1.0, t))


def rounded_rect_sdf(px, py, half_w, half_h, radius):
    dx = abs(px) - (half_w - radius)
    dy = abs(py) - (half_h - radius)
    outside = math.hypot(max(dx, 0.0), max(dy, 0.0))
    inside = min(max(dx, dy), 0.0)
    return outside + inside - radius


def over(dst, src, alpha):
    return tuple(round(s * alpha + d * (1 - alpha)) for d, s in zip(dst, src))


def render(size):
    """Returns RGBA bytes for one square icon of the given pixel size."""
    s = float(size)
    centre = s / 2.0

    # macOS icons leave a margin around the tile rather than filling the square.
    tile_half = s * 0.402
    tile_radius = s * 0.225

    disc_radius = s * 0.20
    ring_radius = s * 0.285
    ring_half_thickness = max(s * 0.017, 0.75)

    rows = []
    for y in range(size):
        row = bytearray()
        py = y + 0.5 - centre
        # Vertical gradient across the tile.
        t = (y + 0.5) / s
        bg = tuple(round(a + (b - a) * t) for a, b in zip(PINK_TOP, PINK_BOTTOM))
        for x in range(size):
            px = x + 0.5 - centre

            tile_cov = smoothstep(0.0, rounded_rect_sdf(px, py, tile_half, tile_half,
                                                        tile_radius))
            if tile_cov <= 0.0:
                row += b"\x00\x00\x00\x00"
                continue

            colour = bg

            # Soft ring, echoing the "today" marker.
            d = math.hypot(px, py)
            ring_cov = smoothstep(0.0, abs(d - ring_radius) - ring_half_thickness)
            if ring_cov > 0.0:
                colour = over(colour, CREAM, ring_cov)

            # Filled rose circle, echoing a logged day.
            disc_cov = smoothstep(0.0, d - disc_radius)
            if disc_cov > 0.0:
                colour = over(colour, ROSE, disc_cov)

            row += bytes(colour) + bytes([round(255 * tile_cov)])
        rows.append(bytes(row))
    return rows


def write_png(path, size, rows):
    raw = b"".join(b"\x00" + row for row in rows)   # filter byte 0 per scanline

    def chunk(tag, data):
        body = tag + data
        return (struct.pack(">I", len(data)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 9)) +
           chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    iconset = os.path.join(ROOT, "assets", "AppIcon.iconset")
    shutil.rmtree(iconset, ignore_errors=True)
    os.makedirs(iconset, exist_ok=True)

    # (pixel size, filename) - macOS wants each size at 1x and 2x.
    wanted = [(16, "icon_16x16.png"),      (32, "icon_16x16@2x.png"),
              (32, "icon_32x32.png"),      (64, "icon_32x32@2x.png"),
              (128, "icon_128x128.png"),   (256, "icon_128x128@2x.png"),
              (256, "icon_256x256.png"),   (512, "icon_256x256@2x.png"),
              (512, "icon_512x512.png"),   (1024, "icon_512x512@2x.png")]

    cache = {}
    for size, name in wanted:
        if size not in cache:
            print(f"  rendering {size}x{size}")
            cache[size] = render(size)
        write_png(os.path.join(iconset, name), size, cache[size])

    icns = os.path.join(ROOT, "assets", "AppIcon.icns")
    subprocess.run(["iconutil", "-c", "icns", iconset, "-o", icns], check=True)
    shutil.rmtree(iconset, ignore_errors=True)
    print(f"wrote {icns} ({os.path.getsize(icns) / 1024:.0f} KB)")


if __name__ == "__main__":
    sys.exit(main())
