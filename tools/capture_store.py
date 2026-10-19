#!/usr/bin/env python3
"""Capture five store screenshots per supported watch using running audit emulators."""

from datetime import datetime, timezone
import json
from pathlib import Path

import capture_matrix


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "store-assets/2026-10-19"
RECIPES = (
    ("01-analogue-classic-gold", "analogue", "classic", "1009", False),
    ("02-numerals-porcelain-blue", "numbered", "porcelain", "1009", False),
    ("03-marks-lagoon-texture", "ticks", "lagoon", "1009", True),
    ("04-hybrid-dusk", "hybrid", "dusk", "1337", False),
    ("05-large-digital-ember", "large", "ember", "1337", False),
)
PLATFORMS = {"emery": (200, 228), "gabbro": (260, 260)}


def main():
    """Save native captures and a manifest describing the upload order."""
    package = json.loads((ROOT / "package.json").read_text())
    assert set(package["pebble"]["targetPlatforms"]) == set(PLATFORMS)
    entries = []
    for platform, dimensions in PLATFORMS.items():
        output = OUTPUT / "screenshots" / platform
        for name, mode, preset, time, texture in RECIPES:
            args = [platform, "--modes", mode, "--preset", preset, "--times", time,
                    "--output", str(output)]
            if texture:
                args.append("--texture")
            capture_matrix.main(args)
            captured = output / f"{platform}-{mode}-{time}.png"
            final = output / f"{name}.png"
            captured.replace(final)
            entries.append({"file": str(final.relative_to(OUTPUT)), "platform": platform,
                            "size": dimensions, "mode": mode, "preset": preset,
                            "time": time[:2] + ":" + time[2:], "texture": texture})
            print(f"Saved {final.relative_to(ROOT)}", flush=True)
    manifest = {"title": "TrioWay", "created_utc": datetime.now(timezone.utc).isoformat(),
                "listing": "https://apps.repebble.com/082418be8cd642a9b06da3ac",
                "requirements": "https://developer.repebble.com/guides/appstore-publishing/preparing-a-submission/",
                "app_version": package["version"], "screenshots": entries}
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
