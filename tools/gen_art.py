#!/usr/bin/env python3
"""Generate demo video stills (192x108 RGB565) -> assets/art.bin + assets/art.h.

These stand in for real video thumbnails until the library scanner exists.
"""
import math
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets")
W, H = 192, 108


def vgrad(size, stops):
    """Vertical gradient from list of (t, (r,g,b))."""
    img = Image.new("RGB", size)
    d = ImageDraw.Draw(img)
    w, h = size
    for y in range(h):
        t = y / max(h - 1, 1)
        for i in range(len(stops) - 1):
            t0, c0 = stops[i]
            t1, c1 = stops[i + 1]
            if t0 <= t <= t1:
                f = (t - t0) / max(t1 - t0, 1e-6)
                col = tuple(int(c0[j] + (c1[j] - c0[j]) * f) for j in range(3))
                d.line([(0, y), (w, y)], fill=col)
                break
    return img


def still_sunset():
    img = vgrad((W, H), [
        (0.0, (38, 24, 76)), (0.35, (120, 52, 108)),
        (0.62, (232, 108, 92)), (0.78, (250, 176, 96)),
        (0.79, (58, 40, 86)), (1.0, (24, 18, 48)),
    ])
    d = ImageDraw.Draw(img, "RGBA")
    d.ellipse([W * 0.62, H * 0.30, W * 0.86, H * 0.66], fill=(255, 226, 160, 235))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([W * 0.50, H * 0.18, W * 0.98, H * 0.80],
                                 fill=(255, 170, 90, 90))
    img = Image.alpha_composite(img.convert("RGBA"), glow.filter(ImageFilter.GaussianBlur(9)))
    d = ImageDraw.Draw(img, "RGBA")
    for i in range(14):
        y = H * 0.80 + i * 2.1
        d.line([(0, y), (W, y)], fill=(255, 190, 130, max(6, 46 - i * 3)), width=1)
    return img


def still_city():
    img = vgrad((W, H), [
        (0.0, (10, 12, 34)), (0.45, (28, 30, 72)),
        (0.62, (128, 74, 120)), (0.72, (240, 132, 110)), (1.0, (16, 14, 30)),
    ])
    d = ImageDraw.Draw(img, "RGBA")
    d.ellipse([W * 0.14, H * 0.12, W * 0.28, H * 0.26], fill=(240, 240, 220, 220))
    import random
    random.seed(7)
    x = 0
    while x < W:
        bw = random.randint(10, 22)
        bh = random.randint(20, 62)
        top = H - bh
        d.rectangle([x, top, x + bw, H], fill=(12, 14, 32, 245))
        for wy in range(top + 4, H - 4, 6):
            for wx in range(x + 3, x + bw - 3, 5):
                if random.random() < 0.45:
                    c = (255, 226, 150, 220) if random.random() < 0.8 else (150, 220, 255, 200)
                    d.rectangle([wx, wy, wx + 2, wy + 2], fill=c)
        x += bw + random.randint(2, 6)
    return img


def still_mountains():
    img = vgrad((W, H), [
        (0.0, (198, 214, 228)), (0.45, (150, 178, 200)),
        (0.7, (206, 214, 218)), (1.0, (232, 230, 224)),
    ])
    layers = [
        ((86, 116, 150), 0.46, 0.55), ((110, 138, 168), 0.58, 0.62),
        ((138, 160, 184), 0.70, 0.70), ((170, 186, 200), 0.82, 0.80),
    ]
    for color, base, amp in layers:
        pts = [(0, H)]
        for x in range(0, W + 4, 4):
            t = x / W
            y = H * base + math.sin(t * 7.5 + base * 9) * H * 0.10 * amp \
                + math.sin(t * 17 + 1.3) * H * 0.03
            pts.append((x, y))
        pts.append((W, H))
        d = ImageDraw.Draw(img, "RGBA")
        d.polygon(pts, fill=color + (255,))
        img = img.filter(ImageFilter.GaussianBlur(1)) if base > 0.6 else img
    return img


def still_neon():
    img = Image.new("RGBA", (W, H), (8, 10, 22, 255))
    d = ImageDraw.Draw(img, "RGBA")
    for k, (col, ph) in enumerate([((64, 232, 255), 0.0), ((196, 92, 255), 1.7),
                                   ((255, 96, 168), 3.1)]):
        pts = []
        for x in range(0, W + 2, 2):
            t = x / W
            y = H * (0.5 + 0.16 * math.sin(t * 6.0 + ph + k * 0.6))
            pts.append((x, y))
        d.line(pts, fill=col + (230,), width=2)
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    glow = img.filter(ImageFilter.GaussianBlur(7))
    img = Image.blend(img, glow, 0.55)
    return img


def still_field():
    img = vgrad((W, H), [
        (0.0, (8, 10, 30)), (0.38, (26, 34, 68)),
        (0.58, (74, 66, 108)), (0.62, (206, 150, 130)),
        (0.63, (26, 30, 26)), (1.0, (10, 14, 12)),
    ])
    import random
    random.seed(3)
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(90):
        x, y = random.uniform(0, W), random.uniform(0, H * 0.55)
        r = random.choice([1, 1, 1, 2])
        a = random.randint(120, 255)
        d.ellipse([x, y, x + r, y + r], fill=(255, 255, 240, a))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([-W * 0.2, H * 0.45, W * 1.2, H * 0.95],
                                 fill=(255, 168, 120, 110))
    img = Image.alpha_composite(img.convert("RGBA"), glow.filter(ImageFilter.GaussianBlur(11)))
    d = ImageDraw.Draw(img, "RGBA")
    for i in range(10):
        y = H * 0.66 + i * 3.5
        d.line([(0, y), (W, y)], fill=(30, 40, 30, 40), width=2)
    return img


def still_abstract():
    img = Image.new("RGB", (W, H), (14, 12, 26))
    blobs = [
        ((52, 30, 120, 200), (270, 210, 255)),
        ((30, 70, 90, 150), (60, 220, 230)),
        ((60, 20, 70, 130), (255, 120, 170)),
    ]
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer, "RGBA")
    for box, col in blobs:
        x0 = box[0] * W / 100
        y0 = box[1] * H / 100
        x1 = box[2] * W / 100
        y1 = box[3] * H / 100
        d.ellipse([x0, y0, x1, y1], fill=col + (200,))
    layer = layer.filter(ImageFilter.GaussianBlur(14))
    img = Image.alpha_composite(img.convert("RGBA"), layer)
    d = ImageDraw.Draw(img, "RGBA")
    for i in range(6):
        y = H * (0.2 + i * 0.12)
        d.line([(0, y), (W, y + 6)], fill=(255, 255, 255, 16), width=1)
    return img


BUILDERS = [still_sunset, still_city, still_mountains,
            still_neon, still_field, still_abstract]


def to_rgb565(img):
    img = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out = bytearray()
    px = img.load()
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out += struct.pack("<H", v)
    return bytes(out)


def main():
    os.makedirs(OUT, exist_ok=True)
    frames = []
    for build in BUILDERS:
        img = build().convert("RGB")
        frames.append(to_rgb565(img))
        img.save(os.path.join(OUT, "art_%s.png" % build.__name__[6:]))

    blob = b"EZAR" + struct.pack("<HHH", len(frames), W, H) + b"".join(frames)
    with open(os.path.join(OUT, "art.bin"), "wb") as f:
        f.write(blob)

    with open(os.path.join(OUT, "art.h"), "w") as f:
        f.write("/* Generated by tools/gen_art.py - do not edit. */\n")
        f.write("#ifndef EZ_ART_DATA_H\n#define EZ_ART_DATA_H\n")
        f.write("#define EZ_ART_COUNT %d\n#define EZ_ART_W %d\n#define EZ_ART_H %d\n"
                % (len(frames), W, H))
        f.write("#endif\n")
    print("art: %d frames, %d bytes" % (len(frames), len(blob)))


if __name__ == "__main__":
    sys.exit(main())
