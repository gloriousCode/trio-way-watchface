#!/usr/bin/env python3
"""Generate native-resolution Pebble digit masks from the bundled OFL font.

Requires Pillow. Run from any directory; output is confined to this repository.
Four-times supersampling is quantised to Pebble's four supported alpha levels.
The SDK converts these PNGs to 2-bit palettised resources without scaling.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "resources/fonts/BebasNeue-Regular.ttf"
OUTPUT = ROOT / "resources/images"
SCALE = 4
PADDING = 2
HEIGHTS = {"emery": {"small": 24, "large": 60}, "gabbro": {"small": 28, "large": 76}}


def save_palette(mask, destination):
    """Store coverage as four alpha levels, retaining transparent padding."""
    quantised = mask.point(lambda value: min(3, (value * 3 + 127) // 255))
    palettised = Image.new("P", mask.size)
    palettised.putdata(quantised.tobytes())
    palettised.putpalette([255, 255, 255] * 4 + [0, 0, 0] * 252)
    palettised.save(destination, bits=2, transparency=bytes((0, 85, 170, 255)))


def generate_style(platform, style, height):
    """Rasterise ten equally spaced digits and a one-pixel outline mask."""
    # Measure a large master so the requested height means visible glyph pixels,
    # rather than the font's em size (which includes unused ascender space).
    font = ImageFont.truetype(str(FONT), 512)
    boxes = [font.getbbox(str(digit)) for digit in range(10)]
    top = min(box[1] for box in boxes)
    bottom = max(box[3] for box in boxes)
    ratio = height * SCALE / (bottom - top)
    widths = [max(1, round((box[2] - box[0]) * ratio)) for box in boxes]
    cell_width = (max(widths) + SCALE - 1) // SCALE + 2 * PADDING
    cell_height = height + 2 * PADDING
    strip = Image.new("L", (cell_width * SCALE * 10, cell_height * SCALE))
    for digit, (box, width) in enumerate(zip(boxes, widths)):
        glyph = Image.new("L", (box[2] - box[0], bottom - top))
        ImageDraw.Draw(glyph).text((-box[0], -top), str(digit), font=font, fill=255)
        glyph = glyph.resize((width, height * SCALE), Image.Resampling.LANCZOS)
        x = digit * cell_width * SCALE + (cell_width * SCALE - width) // 2
        strip.paste(glyph, (x, PADDING * SCALE))
    outline = strip.filter(ImageFilter.MaxFilter(2 * SCALE + 1))
    size = (cell_width * 10, cell_height)
    for name, mask in (("fill", strip), ("outline", outline)):
        mask = mask.resize(size, Image.Resampling.LANCZOS)
        save_palette(mask, OUTPUT / f"digits_{style}_{name}~{platform}.png")
    print(f"{platform} {style}: {cell_width} x {cell_height} px cells; "
          f"{2 * ((cell_width * 10 + 3) // 4) * cell_height} bitmap bytes")


def main():
    """Regenerate the checked-in assets deterministically with Pillow."""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for platform, styles in HEIGHTS.items():
        for style, height in styles.items():
            generate_style(platform, style, height)


if __name__ == "__main__":
    main()
