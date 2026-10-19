"""Validate checked-in digit masks at their native Pebble resolution (Pillow)."""

import json
from pathlib import Path
import unittest

from PIL import Image, ImageChops


ROOT = Path(__file__).resolve().parents[1]


class DigitAssetsTest(unittest.TestCase):
    def test_native_masks_and_coverage(self):
        heights = {"emery": {"small": 24, "large": 60},
                   "gabbro": {"small": 28, "large": 76}}
        for platform, styles in heights.items():
            for style, height in styles.items():
                with self.subTest(platform=platform, style=style):
                    masks = []
                    for kind in ("fill", "outline"):
                        path = ROOT / f"resources/images/digits_{style}_{kind}~{platform}.png"
                        with Image.open(path) as image:
                            self.assertEqual(image.mode, "P")
                            self.assertEqual(image.height, height + 4)
                            self.assertEqual(image.width % 10, 0)
                            self.assertEqual(image.info["transparency"], bytes((0, 85, 170, 255)))
                            self.assertEqual(set(image.tobytes()), {0, 1, 2, 3})
                            alpha = image.convert("RGBA").getchannel("A")
                            masks.append(alpha)
                            cell_width = image.width // 10
                            for digit in range(10):
                                cell = alpha.crop((digit * cell_width, 0, (digit + 1) * cell_width, image.height))
                                self.assertIsNotNone(cell.getbbox())
                                self.assertIsNone(cell.crop((0, 0, cell_width, 1)).getbbox())
                                self.assertIsNone(cell.crop((0, image.height - 1, cell_width, image.height)).getbbox())
                                self.assertIsNone(cell.crop((0, 0, 1, image.height)).getbbox())
                                self.assertIsNone(cell.crop((cell_width - 1, 0, cell_width, image.height)).getbbox())
                    self.assertEqual(masks[0].size, masks[1].size)
                    self.assertIsNone(ImageChops.subtract(masks[0], masks[1]).getbbox())
                    self.assertIsNotNone(ImageChops.subtract(masks[1], masks[0]).getbbox())

    def test_resources_keep_four_alpha_levels(self):
        package = json.loads((ROOT / "package.json").read_text())
        entries = {entry["name"]: entry for entry in package["pebble"]["resources"]["media"]}
        for style in ("SMALL", "LARGE"):
            for kind in ("FILL", "OUTLINE"):
                entry = entries[f"DIGITS_{style}_{kind}"]
                self.assertEqual(entry["type"], "bitmap")
                self.assertEqual(entry["memoryFormat"].lower(), "2bitpalette")
                self.assertEqual(entry["storageFormat"].lower(), "pbi")
                self.assertEqual(entry["file"], f"images/digits_{style.lower()}_{kind.lower()}.png")


if __name__ == "__main__":
    unittest.main()
