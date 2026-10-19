#!/usr/bin/env python3
"""Export the TrioWay geometric store mark at both dashboard icon sizes."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT.parent / (ROOT.name + "-assets") / "store-assets/2026-10-19/icons"
# Three sectors meet at the hand pivot; the lower seam shares its exact x.
SECTORS = (
    ("#17191b", ((0, 0), (144, 0), (144, 20), (72, 78), (0, 29))),
    ("#55585b", ((0, 29), (72, 78), (72, 144), (0, 144))),
    ("#aaaeb1", ((72, 78), (144, 20), (144, 144), (72, 144))),
)
HANDS = (("#ffffff", (39, 55), 6), ("#ffaa55", (119, 40), 5))


def main():
    """Render opaque PNGs with antialiased edges and retain an editable SVG."""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 144 144">']
    for colour, points in SECTORS:
        coords = " ".join(f"{x},{y}" for x, y in points)
        svg.append(f'<polygon fill="{colour}" points="{coords}"/>')
    for colour, (x, y), width in HANDS:
        for stroke, thickness in (("#17191b", width + 3), (colour, width)):
            svg.append(f'<path d="M72 78L{x} {y}" stroke="{stroke}" '
                       f'stroke-width="{thickness}" stroke-linecap="round"/>')
    svg.extend(['<circle cx="72" cy="78" r="4.5" fill="#17191b"/>',
                '<circle cx="72" cy="78" r="2.5" fill="#ffaa55"/>', '</svg>'])
    (OUTPUT / "trioway-icon.svg").write_text("\n".join(svg) + "\n")
    for size in (80, 144):
        scale = size * 8 / 144
        canvas = Image.new("RGB", (size * 8, size * 8))
        draw = ImageDraw.Draw(canvas)
        for colour, points in SECTORS:
            draw.polygon([(round(x * scale), round(y * scale)) for x, y in points], fill=colour)
        for colour, endpoint, width in HANDS:
            points = ((72, 78), endpoint)
            for stroke, thickness in (("#17191b", width + 3), (colour, width)):
                draw.line([(round(x * scale), round(y * scale)) for x, y in points],
                          fill=stroke, width=round(thickness * scale))
                for x, y in points:
                    radius = thickness / 2
                    draw.ellipse(((x - radius) * scale, (y - radius) * scale,
                                  (x + radius) * scale, (y + radius) * scale), fill=stroke)
        for radius, colour in ((4.5, "#17191b"), (2.5, "#ffaa55")):
            draw.ellipse(((72 - radius) * scale, (78 - radius) * scale,
                          (72 + radius) * scale, (78 + radius) * scale), fill=colour)
        target = OUTPUT / f"trioway-icon-{size}x{size}.png"
        canvas.resize((size, size), Image.Resampling.LANCZOS).save(target, optimize=True)
        print(target)


if __name__ == "__main__":
    main()
