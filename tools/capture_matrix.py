#!/usr/bin/env python3
"""Capture deterministic test builds using an already-running local emulator.

No emulator is started, stopped, reset or wiped. The existing debug source is
restored in finally, then a normal build is produced. The release is reinstalled
only after all captures succeed, so a failed transport is not retried implicitly.
"""

import argparse
from contextlib import contextmanager
import fcntl
import json
from pathlib import Path
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT.parent / (ROOT.name + "-assets")
DEBUG = ROOT / "src/c/mdbl_debug.c"
MODES = {"analogue": "FACE_MODE_HANDS", "hybrid": "FACE_MODE_DIGITAL",
         "large": "FACE_MODE_LARGE_DIGITAL", "numbered": "FACE_MODE_NUMBERED", "ticks": "FACE_MODE_TICKS"}


@contextmanager
def capture_lock():
    """Exclude concurrent captures before editing shared sources or build output."""
    lock_path = ROOT / "build/capture.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a") as lock:
        try:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another capture holds build/capture.lock; wait for it to finish") from error
        try:
            yield
        finally:
            fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def run(*args):
    """Run one command in this checkout, preserving diagnostics on failure."""
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)


def main(argv=None):
    """Build and capture representative times, dates and layouts."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("emery", "gabbro"))
    parser.add_argument("--port", type=int, help="Defaults to 12344 for Emery, 12345 for Gabbro")
    parser.add_argument("--modes", default="analogue,numbered,ticks,hybrid,large")
    parser.add_argument("--output", type=Path, default=ASSET_ROOT / "mockups/current",
                        help="Destination inside the sibling time-2-design-assets archive")
    parser.add_argument("--preset", help="Preset key from constants.js")
    texture = parser.add_mutually_exclusive_group()
    texture.add_argument("--texture", action="store_true", help="Enable the production texture setting")
    texture.add_argument("--texture-preview", action="store_true", help="Render the original shading prototype for comparison")
    parser.add_argument("--times", default="0000,0808,1010,1337,2359",
                        help="Comma-separated capture times from the fixed test set")
    args = parser.parse_args(argv)
    if args.port is not None and not 1 <= args.port <= 65535:
        parser.error("Port must be between 1 and 65535")
    selected_times = args.times.split(",")
    selected_modes = args.modes.split(",")
    if not set(selected_times) <= {"0000", "0315", "0630", "0808", "0945", "1009", "1010", "1337", "2359"}:
        parser.error("Unsupported time; choose from 0000,0315,0630,0808,0945,1009,1010,1337,2359")
    if not set(selected_modes) <= MODES.keys():
        parser.error("Unsupported mode; choose from analogue,numbered,ticks,hybrid,large")
    host = f"localhost:{args.port or (12344 if args.platform == 'emery' else 12345)}"
    output = args.output.resolve()
    if not output.is_relative_to(ASSET_ROOT.resolve()) or output.is_relative_to(ROOT):
        parser.error("Capture output must be inside the sibling asset archive, outside this checkout")
    args.colours = None
    if args.preset:
        result = subprocess.run(["node", "-e", "process.stdout.write(JSON.stringify(require('./src/pkjs/constants').PRESETS))"],
                                cwd=ROOT, capture_output=True, text=True, check=True)
        presets = json.loads(result.stdout)
        if args.preset not in presets:
            parser.error("Unknown preset: " + args.preset)
        fields = {"bgColour": "bg_colour", "hourColour": "hour_colour", "minuteColour": "minute_colour",
                  "hourHandColour": "hour_hand_colour", "minuteHandColour": "minute_hand_colour",
                  "complicationBgColour": "complication_bg_colour", "complicationBorderColour": "complication_border_colour",
                  "complicationTextColour": "complication_text_colour"}
        args.colours = "\n".join(f"  state->{field} = GColorFromHEX(0x{presets[args.preset][key][1:]});" for key, field in fields.items())
    with capture_lock():
        capture_selected(args, selected_times, selected_modes, host, output)


def capture_selected(args, selected_times, selected_modes, host, output):
    """Capture the validated selection while the caller holds the build lock."""
    output.mkdir(parents=True, exist_ok=True)
    complete = False
    original = DEBUG.read_text()
    renderer = ROOT / "src/c/mdbl_render.c"
    original_render = renderer.read_text() if args.texture_preview else None
    try:
        if original_render is not None:
            snippet = (ROOT / "tools/texture_preview.inc").read_text()
            anchor = "  draw_sector_seams(ctx, centre, radius, hour, minute, split);"
            if original_render.count("void update_canvas(") != 1 or original_render.count(anchor) != 1:
                raise RuntimeError("Renderer preview anchors changed; refusing an incomplete texture capture")
            modified = original_render.replace("void update_canvas(", snippet + "\nvoid update_canvas(")
            modified = modified.replace(anchor, anchor + "\n  draw_texture_preview(ctx, bounds, centre, rays);")
            renderer.write_text(modified)
        for hour, minute, day in ((0, 0, 1), (3, 15, 19), (6, 30, 19), (8, 8, 28), (9, 45, 19), (10, 9, 19), (10, 10, 19), (13, 37, 31), (23, 59, 30)):
            if f"{hour:02d}{minute:02d}" not in selected_times:
                continue
            for mode, value in MODES.items():
                if mode not in selected_modes:
                    continue
                DEBUG.write_text(f'''#include "mdbl.h"
void apply_debug_state(AppState *state) {{
  state->mode = {value};
{args.colours or ""}
  state->show_hands = true;
  state->show_texture = {str(args.texture).lower()};
  state->show_date_complication = true;
  state->show_date_tiles = {str(mode == "hybrid").lower()};
  state->now.tm_hour = {hour};
  state->now.tm_min = {minute};
  state->now.tm_sec = 0;
  state->now.tm_mday = {day};
  state->now.tm_wday = 3;
}}
bool should_ignore_config_messages(void) {{ return true; }}
''')
                run("pebble", "build")
                run("pebble", "install", "--qemu", host, "build/time-2-design.pbw")
                # The OS launch animation can outlast the install command.
                # Earlier captures at 0.3s caught its skewed white frame.
                time.sleep(2)
                name = f"{args.platform}-{mode}-{hour:02d}{minute:02d}.png"
                run("pebble", "screenshot", "--qemu", host, "--no-open", str(output / name))
                print(name, flush=True)
        complete = True
    finally:
        if original_render is not None:
            renderer.write_text(original_render)
        DEBUG.write_text(original)
        run("pebble", "build")
        if complete:
            run("pebble", "install", "--qemu", host, "build/time-2-design.pbw")


if __name__ == "__main__":
    main()
