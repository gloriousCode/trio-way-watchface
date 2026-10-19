"""Test capture orchestration without launching processes or writing files."""

from contextlib import ExitStack, nullcontext, redirect_stderr, redirect_stdout
import io
from pathlib import Path
import types
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/capture_matrix.py"
capture = types.ModuleType("capture_matrix")
capture.__file__ = str(SCRIPT)
exec(compile(SCRIPT.read_text(), str(SCRIPT), "exec"), capture.__dict__)
ORIGINAL = "// Original debug source, including any existing user changes.\n"


class CaptureMatrixTest(unittest.TestCase):
    def harness(self, stack, failure_stage=None, error_type=RuntimeError):
        debug = Mock()
        debug.read_text.return_value = ORIGINAL
        current = {"source": ORIGINAL}
        commands = []

        def write_source(source):
            current["source"] = source

        def run_command(*args):
            commands.append((args, current["source"]))
            if args[1] == failure_stage and current["source"] != ORIGINAL:
                raise error_type("simulated capture failure")

        debug.write_text.side_effect = write_source
        stack.enter_context(patch.object(capture, "DEBUG", debug))
        stack.enter_context(patch.object(capture, "capture_lock", return_value=nullcontext()))
        runner = stack.enter_context(patch.object(capture, "run", side_effect=run_command))
        mkdir = stack.enter_context(patch.object(Path, "mkdir"))
        sleep = stack.enter_context(patch.object(capture.time, "sleep"))
        stack.enter_context(redirect_stdout(io.StringIO()))
        stack.enter_context(redirect_stderr(io.StringIO()))
        return debug, runner, mkdir, sleep, commands

    def test_invalid_arguments_fail_before_source_or_output_changes(self):
        arguments = (
            ["invalid"],
            ["emery", "--times", "1260"],
            ["emery", "--times", ""],
            ["emery", "--modes", "unknown"],
            ["emery", "--modes", ""],
            ["emery", "--texture", "--texture-preview"],
            ["emery", "--port", "not-a-number"],
            ["emery", "--port", "0"],
            ["emery", "--port", "-1"],
            ["emery", "--port", "65536"],
            ["emery", "--output", str(ROOT.parent / "outside-checkout")],
            ["emery", "--output", str(ROOT / "mockups/current")],
            ["emery", "--output", str(capture.ASSET_ROOT / "../outside-archive")],
        )
        for argv in arguments:
            with self.subTest(argv=argv), ExitStack() as stack:
                debug, runner, mkdir, sleep, commands = self.harness(stack)
                with self.assertRaises(SystemExit) as raised:
                    capture.main(argv)
                self.assertEqual(raised.exception.code, 2)
                debug.read_text.assert_not_called()
                debug.write_text.assert_not_called()
                runner.assert_not_called()
                mkdir.assert_not_called()
                sleep.assert_not_called()
                self.assertEqual(commands, [])

    def test_success_uses_selected_platform_modes_times_and_restores_release(self):
        platforms = (("emery", [], 12344), ("gabbro", [], 12345),
                     ("gabbro", ["--port", "23456"], 23456))
        for platform, options, port in platforms:
            with self.subTest(platform=platform, port=port), ExitStack() as stack:
                debug, runner, mkdir, sleep, commands = self.harness(stack)
                capture.main([platform, "--modes", "hybrid,large", "--times", "0808,2359", *options])
                host = f"localhost:{port}"
                expected = []
                for hour, minute, day in ((8, 8, 28), (23, 59, 30)):
                    for mode, constant in (("hybrid", "FACE_MODE_DIGITAL"),
                                           ("large", "FACE_MODE_LARGE_DIGITAL")):
                        offset = len(expected)
                        expected.extend([
                            ("pebble", "build"),
                            ("pebble", "install", "--qemu", host, "build/time-2-design.pbw"),
                            ("pebble", "screenshot", "--qemu", host, "--no-open",
                             str(capture.ASSET_ROOT / "mockups/current" / f"{platform}-{mode}-{hour:02d}{minute:02d}.png")),
                        ])
                        source = commands[offset][1]
                        self.assertIn(f"state->mode = {constant};", source)
                        self.assertIn(f"state->now.tm_hour = {hour};", source)
                        self.assertIn(f"state->now.tm_min = {minute};", source)
                        self.assertIn(f"state->now.tm_mday = {day};", source)
                        self.assertIn(f"state->show_date_tiles = {str(mode == 'hybrid').lower()};", source)
                        self.assertIn("bool should_ignore_config_messages(void) { return true; }", source)
                        self.assertEqual([entry[1] for entry in commands[offset:offset + 3]], [source] * 3)
                expected.extend([
                    ("pebble", "build"),
                    ("pebble", "install", "--qemu", host, "build/time-2-design.pbw"),
                ])
                self.assertEqual([command for command, _ in commands], expected)
                self.assertEqual(commands[-2][1], ORIGINAL)
                self.assertEqual(commands[-1][1], ORIGINAL)
                debug.write_text.assert_called_with(ORIGINAL)
                mkdir.assert_called_once_with(parents=True, exist_ok=True)
                self.assertEqual(sleep.call_count, 4)
                self.assertTrue(all(call.args[0] >= 2 for call in sleep.call_args_list))

    def test_failure_restores_source_and_builds_without_retrying_transport(self):
        for stage in ("build", "install", "screenshot"):
            with self.subTest(stage=stage), ExitStack() as stack:
                debug, runner, mkdir, sleep, commands = self.harness(stack, failure_stage=stage)
                with self.assertRaisesRegex(RuntimeError, "simulated capture failure"):
                    capture.main(["emery", "--modes", "analogue", "--times", "0000"])
                debug.write_text.assert_called_with(ORIGINAL)
                self.assertEqual(commands[-1], (("pebble", "build"), ORIGINAL))
                expected_stages = ["build", "install", "screenshot"]
                expected_stages = expected_stages[:expected_stages.index(stage) + 1] + ["build"]
                self.assertEqual([command[1] for command, _ in commands], expected_stages)
                self.assertFalse(any(command[1] == "install" and source == ORIGINAL
                                     for command, source in commands))

    def test_keyboard_interrupt_restores_source_and_builds_release(self):
        with ExitStack() as stack:
            debug, runner, mkdir, sleep, commands = self.harness(
                stack, failure_stage="screenshot", error_type=KeyboardInterrupt)
            with self.assertRaises(KeyboardInterrupt):
                capture.main(["gabbro", "--modes", "hybrid", "--times", "1010"])
            debug.write_text.assert_called_with(ORIGINAL)
            self.assertEqual(commands[-1], (("pebble", "build"), ORIGINAL))
            self.assertEqual([command[1] for command, _ in commands],
                             ["build", "install", "screenshot", "build"])

    def test_run_preserves_command_diagnostics_and_checkout(self):
        result = types.SimpleNamespace(returncode=1, stdout="build output\n", stderr="specific failure\n")
        with patch.object(capture.subprocess, "run", return_value=result) as process:
            with self.assertRaisesRegex(RuntimeError, "build output\nspecific failure"):
                capture.run("pebble", "build")
            process.assert_called_once_with(("pebble", "build"), cwd=ROOT,
                                            capture_output=True, text=True)

    def test_texture_preview_restores_renderer_on_success_and_every_failure(self):
        renderer = ROOT / "src/c/mdbl_render.c"
        original = "void update_canvas(\n  draw_sector_seams(ctx, centre, radius, hour, minute, split);\n"
        for failure in (None, "build", "install", "screenshot"):
            with self.subTest(failure=failure), ExitStack() as stack:
                debug, runner, mkdir, sleep, commands = self.harness(stack, failure_stage=failure)
                stack.enter_context(patch.object(Path, "read_text", autospec=True,
                    side_effect=lambda path: original if path == renderer else "// Preview helper"))
                writer = stack.enter_context(patch.object(Path, "write_text", autospec=True))
                argv = ["emery", "--modes", "ticks", "--times", "1009", "--texture-preview"]
                if failure:
                    with self.assertRaisesRegex(RuntimeError, "simulated capture failure"):
                        capture.main(argv)
                else:
                    capture.main(argv)
                writes = [call.args[1] for call in writer.call_args_list if call.args[0] == renderer]
                self.assertEqual(len(writes), 2)
                self.assertIn("// Preview helper", writes[0])
                self.assertIn("draw_texture_preview(ctx, bounds, centre, rays);", writes[0])
                self.assertEqual(writes[-1], original)
                self.assertEqual(commands[-1 if failure else -2][1], ORIGINAL)

    def test_texture_setting_is_explicit_for_flat_and_textured_captures(self):
        for option, enabled in (([], False), (["--texture"], True)):
            with self.subTest(enabled=enabled), ExitStack() as stack:
                debug, runner, mkdir, sleep, commands = self.harness(stack)
                capture.main(["emery", "--times", "1009", "--modes", "ticks", *option])
                self.assertIn(f"state->show_texture = {str(enabled).lower()};", commands[0][1])
                debug.write_text.assert_called_with(ORIGINAL)

    def test_texture_preview_rejects_changed_anchors_before_debug_override(self):
        with ExitStack() as stack:
            debug, runner, mkdir, sleep, commands = self.harness(stack)
            stack.enter_context(patch.object(Path, "read_text", return_value="changed renderer"))
            writer = stack.enter_context(patch.object(Path, "write_text", autospec=True))
            with self.assertRaisesRegex(RuntimeError, "preview anchors changed"):
                capture.main(["emery", "--texture-preview"])
            writer.assert_called_once_with(ROOT / "src/c/mdbl_render.c", "changed renderer")
            debug.write_text.assert_called_once_with(ORIGINAL)
            self.assertEqual(commands, [(("pebble", "build"), ORIGINAL)])

    def test_capture_lock_is_exclusive_nonblocking_and_released_on_failure(self):
        with patch.object(Path, "mkdir"), patch.object(Path, "open") as open_lock, \
                patch.object(capture.fcntl, "flock") as flock:
            handle = open_lock.return_value.__enter__.return_value
            handle.fileno.return_value = 123
            with self.assertRaisesRegex(ValueError, "capture failed"):
                with capture.capture_lock():
                    raise ValueError("capture failed")
            open_lock.assert_called_once_with("a")
            self.assertEqual([call.args for call in flock.call_args_list], [
                (123, capture.fcntl.LOCK_EX | capture.fcntl.LOCK_NB),
                (123, capture.fcntl.LOCK_UN),
            ])

    def test_busy_lock_does_not_enter_capture_or_release_another_lock(self):
        with patch.object(Path, "mkdir"), patch.object(Path, "open"), \
                patch.object(capture.fcntl, "flock", side_effect=BlockingIOError) as flock:
            with self.assertRaisesRegex(RuntimeError, "Another capture holds"):
                with capture.capture_lock():
                    self.fail("A busy lock must not enter the capture body")
            self.assertEqual(flock.call_count, 1)

    def test_main_acquires_lock_before_reading_or_editing_source(self):
        with ExitStack() as stack:
            debug, runner, mkdir, sleep, commands = self.harness(stack)
            with patch.object(capture, "capture_lock", side_effect=RuntimeError("busy lock")):
                with self.assertRaisesRegex(RuntimeError, "busy lock"):
                    capture.main(["emery"])
            debug.read_text.assert_not_called()
            debug.write_text.assert_not_called()
            runner.assert_not_called()
            mkdir.assert_not_called()


if __name__ == "__main__":
    unittest.main()
