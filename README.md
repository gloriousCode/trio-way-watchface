# TrioWay

TrioWay is a three-colour Pebble watchface with plain analogue, numbered analogue, marked analogue, hybrid and large digital layouts.

It focuses on clean geometry, readable time, and calm muted palettes rather than feature-heavy complication stacks.

## Layouts

- **Analogue:** the original hands and three colour sectors.
- **Analogue · Numerals:** a full unboxed 1–12 dial. The current hour numeral uses the hour-hand colour; the nearest five-minute numeral uses the minute-hand colour (13:37 highlights 1 and 7). If both select one numeral, its hour-coloured digits gain a small minute-coloured underline. The hands stop before the numerals. Day/date tiles become a compact interior badge.
- **Analogue · Marks:** 12 longer hour marks and 48 short minute marks follow the display perimeter. The hands keep their true angles and stop before the marks. The date badge prefers the lower half and moves clear of hands and marks.
- **Hybrid:** replaces Digital Small. Upright hour and minute numbers both sit at the outer edge, so 13:37 reads as `13` and `37` without counting dial positions. The numerals have no filled boxes; dark colours receive a thin light outline and light colours a dark outline for contrast. When the hands are close, the labels spread along the perimeter to avoid overlap; the hands retain their actual time directions. Optional date information moves clear of both labels and the full length of both hands. Day/date tiles become one compact weekday/date badge in this mode.
- **Digital Large:** a fixed hour-over-minute readout. Native-size, antialiased Bebas Neue numerals keep their position stable as the background sectors move.

Saved `digital` settings select Hybrid automatically. The hand visibility toggle applies to all three analogue layouts; Hybrid always includes its hands. Numbered Analogue and both digital modes use the individual hand colours for their highlighted numerals.

The minute hand is longer and slimmer than the hour hand (nominal lengths 42% and 24% of the smaller display dimension). At 10:09 the hand angles are 54° and 304.5°, measured clockwise from noon. Labels and marks shorten the hands only along those same rays.

## Colours and texture

The revised catalogue uses seven neutral palettes: Classic · Gold, Graphite, Silver, Paper, Ink · Blue, Stone · Gold and Porcelain · Blue. Three optional tonal palettes use a single colour family: Navy, Forest and Burgundy. Three more creative palettes add coordinated contrasts: Dusk combines midnight blue, charcoal and mauve with a gold hand; Lagoon pairs deep blue and teal with seafoam and a pale yellow hand; Ember combines charcoal, deep red and sand with a blush hand. Sector tones are explicit native Pebble colours; hand highlights and date badges are coordinated rather than generated. Existing saved colours remain unchanged until a preset is selected.

**Subtle background texture** is optional and off by default. It uses sparse, static ordered dithering to suggest translucent shading beside the sector boundaries. The 64-colour display makes this a fine grain rather than a smooth OLED gradient. The texture follows existing redraws; it adds no timer, animation, sensor subscription or heap allocation. It does add drawing work when enabled, and its battery cost has not been measured on hardware. The setting persists in a spare bit of the existing 11-byte record; older records load with texture disabled.

## Rendering and battery audit — 19 October 2026

The watchface already avoided seconds updates, sensor subscriptions and animations. This pass keeps that minute-only behaviour and uses the native minute tick service, restores settings from a small on-watch record, and skips redraws and flash writes for unchanged settings. Startup phone synchronisation remains for configuration migration and recovery. Superseded retry messages are discarded before transmission.

The SDK's custom-font converter rasterises glyphs to one bit; line antialiasing cannot smooth those fonts. The digital numerals now use four alpha levels in native-resolution, two-bit bitmap masks generated from the bundled OFL font. Only the active size is loaded, and repeated digit draws reuse the same resources. The largest masks occupy about 12 KiB on Emery and 18 KiB on Gabbro, plus bitmap metadata. Plain and Marked Analogue do not load digit masks; Numbered Analogue reuses the small masks and still redraws only once per minute.

Sector polygons no longer allocate application-owned paths on each redraw or repaint the background-coloured sector unnecessarily. Antialiased polygon fills and thin antialiased strokes on all three boundaries replace thick seam patches. The SDK may still allocate internally while drawing paths. There is no measured physical-watch battery-life claim.

### Current API documentation

The official SDK page lists [4.33.1](https://developer.repebble.com/sdk/changelogs/4.33.1/) as the latest release checked for this audit; the development setup has been updated from SDK 4.9.169 / CLI 5.0.38 to SDK 4.33.1 / CLI 5.0.40. [SDK 4.33](https://developer.repebble.com/sdk/changelogs/4.33/) adds touch recognisers, alarm lookup and HRV APIs. Those features are not required for this watchface. The manifest's `sdkVersion` correctly remains `"3"`, as specified by the [installation guide](https://developer.repebble.com/sdk/).

Relevant contracts: [minute ticks](https://developer.repebble.com/docs/c/Foundation/Event_Service/TickTimerService/), [persistent storage](https://developer.repebble.com/docs/c/Foundation/Storage/), [bitmap transparency](https://developer.repebble.com/docs/c/Graphics/Graphics_Types/), and [battery guidance](https://developer.repebble.com/guides/best-practices/conserving-battery-life/).

### Building and checking

Run `pebble build`; the current SDK installation has a working Python environment. If a newly appended message key is missing from the generated header after changing `package.json`, remove only `build/include/message_keys.auto.h` and `build/src/message_keys.auto.c`, then rebuild; this regenerates the SDK’s cached key declarations without touching emulator flash. If an existing macOS SDK is missing its Python virtualenv, `sh tools/build_local.sh` uses the already-installed Pebble tool Python and ARM compiler without modifying the SDK. Override `PEBBLE_SDK_ROOT` and `PEBBLE_SDK_PYTHON` for other installations.

Run `sh tools/test.sh` for the Node, native C and asset checks. Asset tests need Pillow; the runner also checks the standard uv Pebble-tool environment. Set `PEBBLE_ASSET_PYTHON` for another interpreter. The C harness uses mocked Pebble APIs, covers settings validation, resource lifetime/failure handling, every minute's hybrid layout, and sector coverage. It does not emulate SDK rasterisation or establish hardware power consumption.

Regenerate the checked-in numeral masks with `python3 tools/generate_digits.py` (Pillow required). Sizes are selected per platform; the watch never scales glyph bitmaps at runtime.

`tools/capture_matrix.py` captures deterministic builds through an already-running local emulator. It temporarily replaces the existing debug hook, restores it in `finally`, and rebuilds the normal bundle. It waits two seconds after installation so screenshots do not catch the OS launch animation. A lock excludes concurrent capture scripts; do not run an independent build or edit sources during capture. It never starts, stops or resets an emulator. Failed transfers abort the run without implicit retries. Use `--preset navy` to select a catalogue palette and `--texture` to capture the production texture. The original approved prototype remains available through `--texture-preview`; this also restores the renderer source in `finally`. Outputs must remain inside this checkout. Rebuild the local palette comparison with `node tools/build_palette_gallery.js` after capturing both platforms for each preset; the gallery switches between native Time 2 and Round 2 images.

### Emulator maintenance

The old SDK emulator stalled during resource transfers even after a restart with the same flash. Updating the SDK and CLI, then starting with the matching SDK's seed flash, restored installs. The old flash is retained under `build/emulator/` for diagnosis. This identifies a working recovery; it does not isolate an upstream firmware defect or guarantee that every future failure has the same cause.

The project-local helper manages only its own recorded process and flash. It checks the PID, command and start time before stopping, rejects occupied ports and unknown owners, and preserves timestamped flash backups on explicit reset. `status` reports process ownership, not protocol health; a successful install plus screenshot checks the transport.

```sh
python3 tools/emulator.py start emery
pebble build
pebble install --qemu localhost:12344 build/time-2-design.pbw
python3 tools/capture_matrix.py emery
python3 tools/emulator.py stop emery
```

Use `gabbro` and port `12345` for the round watch. Captures go to `../time-2-design-assets/mockups/current/`; `--times 1337,2359 --modes hybrid,large` selects a shorter run. For a stalled test emulator, explicitly run `python3 tools/emulator.py reset emery`, then `start emery` and retry one install. Reset leaves it stopped and preserves its previous flash. Diagnostics are `emulator.log` and `serial.log` in `build/emulator/<platform>/`. Avoid overlapping install commands or repeatedly retrying a timed-out transfer. No global `pebble kill` or `pebble wipe` is needed.

For later tool updates, use `uv tool upgrade pebble-tool`, `pebble sdk install latest`, and `pebble sdk activate <installed-version>`. Reset test flash before switching firmware versions. Override `PEBBLE_SDK_ROOT` for the helper/build fallback when using a nonstandard SDK location.

### Validation

`sh tools/test.sh` covers 20 JavaScript checks, 161 native settings cases, bitmap lifecycle/failure tests, 2,880 sector layouts with 3,082,560 interior samples, and 8,640 hybrid layouts. Numbered-dial tests cover all minutes, both displays, numeral highlighting and hand clearance. Marked-dial tests cover all 60 marks, stroke widths, clipping and cached redraws. The shared date placement passes 34,560 layouts across all minutes, four dial modes, both displays/date sizes, and shown/hidden hands. Texture tests compare 92 cases pixel-for-pixel with the approved prototype, including all native palette colours and repeat stability. Seventeen emulator lifecycle tests, eleven capture workflow tests and two asset tests cover the development tools and resource constraints.

Both targets build with SDK 4.33.1. Emulator captures check the revised hands/dates at 00:00, 03:15, 06:30, 09:45, 10:09 and 13:37 on Emery, with corresponding round-display checks at all except 09:45. Texture captures cover every mode on Emery and midnight/10:09 on Gabbro. All 13 palettes were captured at native resolution on both Time 2 and Round 2, with palette limits and round-corner clipping verified. Dusk, Lagoon and Ember also received textured Round 2 checks in Marked Analogue, Hybrid and Large Digital at 13:37. The production Emery capture matches the approved prototype pixel-for-pixel. A normal-release AppMessage check enabled texture, reinstalled the app, and captured an identical image; disabling texture then changed 3,439 pixels. Both audit emulators were stopped after validation. Capture scripts restore the debug source and rebuild/reinstall the normal release afterwards. The existing SDK linker warning about an RWX LOAD segment remains.

Host tests mock Pebble APIs; they do not substitute for rendered emulator checks or physical-watch battery measurements. Battery savings have not been measured on a physical watch.

## Supported Watches

- Pebble Time 2 (`emery`)
- Pebble Round 2 (`gabbro`)

## Why I Designed It

I designed TrioWay because most digital watchfaces felt either busy or plain.

I wanted something that still felt like a proper watch at a glance: balanced, attractive, and easy to read. I also wanted the colour combinations to feel intentional and usable day-to-day, not loud for the sake of being loud.

## Screenshot and store archives

Screenshots, preview galleries and store banners live outside this repository in the
sibling `../time-2-design-assets/` directory. Keeping generated assets separate keeps
the source project below CloudPebble's 400-file import limit. Required watchface
images and fonts remain in `resources/`.

The archive preserves the existing directory structure:

- `mockups/presets/index.html`: all thirteen presets on Time 2 and Round 2, including flat/texture comparisons.
- `mockups/current/`: native-resolution captures of all five face layouts.
- `store-assets/2026-10-19/index.html`: store screenshot and banner gallery.
- `store-assets/2026-10-19/trioway-store-assets.zip`: the upload package.

`tools/capture_matrix.py` saves into this sibling archive by default; custom
`--output` paths must also be inside the archive. `tools/capture_store.py` writes
its store set there, and `node tools/build_palette_gallery.js` regenerates the
palette gallery there. These tools still read and build sources in this checkout.
The archive is local and is not included in a fresh Git clone; retain a separate
backup of finished marketing assets. Old preview-generation scripts are retained
in the archive for reference; use the maintained tools in `tools/` for new captures.

The old repository paths are ignored to prevent accidentally adding screenshots
again. When applying this cleanup to an existing clone, commit the tracked asset
removals as well as these tool changes: ignore rules alone do not remove files
already tracked by Git.

## Clock format

In watchface settings, enable **Use 12-hour time** for Hybrid and Digital Large.
Leave it off for 24-hour time (the default). In 12-hour mode, 13:37 displays as
01:37 and midnight/noon as 12:00; no AM/PM label is added. The preference survives
restarts and does not change analogue hand positions or numbered dials.

Settings are grouped into Time & layout, Date, and Colours & finish. Expand
**Customise individual colours** to edit sectors, hands/numerals and date colours.
**Save & close** sends your choices; Cancel leaves unapplied edits behind.
Live Apply is shown only when an emulator callback is available.
