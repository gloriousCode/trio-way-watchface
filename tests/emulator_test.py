#!/usr/bin/env python3
"""Lifecycle safety checks using mocks only: no emulator or signal is issued."""

import importlib.util
import json
from pathlib import Path
import signal
import sys
import unittest
from unittest.mock import MagicMock, patch

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("owned_emulator", Path(__file__).resolve().parents[1] / "tools/emulator.py")
emulator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(emulator)


class EmulatorTests(unittest.TestCase):
    """Keep PID identity, flash ownership and explicit reset boundaries fail-closed."""

    def setUp(self):
        self.directory = emulator.ROOT / "build/emulator/emery"
        self.record = {
            "pid": 321, "port": 12344, "started": "Mon Oct 19 12:00:00 2026",
            "command": ["/sdk/qemu-pebble", "-machine", "pebble-emery", "-drive",
                        f"if=mtd,format=raw,file={self.directory / 'flash.bin'}"]}
        self.identity = (" ".join(self.record["command"]), self.record["started"])

    def test_platform_and_path_validation(self):
        for platform in ("emery", "gabbro"):
            self.assertEqual(emulator.data_directory(platform), emulator.ROOT / "build/emulator" / platform)
        for platform in ("basalt", "../emery", "/outside", ""):
            with self.subTest(platform=platform), self.assertRaises(ValueError):
                emulator.data_directory(platform)
        with patch.object(Path, "resolve", return_value=Path("/outside")), self.assertRaises(RuntimeError):
            emulator.data_directory("emery")
        with patch.object(Path, "is_symlink", return_value=True), self.assertRaises(RuntimeError):
            emulator.checked_file(self.directory, "flash.bin")

    def test_record_validation_requires_exact_flash_and_platform(self):
        records = [self.record, {**self.record, "pid": 1}, {**self.record, "pid": True},
                   {**self.record, "started": ""}, {**self.record, "port": 80},
                   {**self.record, "command": ["echo", str(self.directory / "flash.bin")]},
                   {**self.record, "command": [arg.replace("flash.bin", "flash.bin.other")
                                               for arg in self.record["command"]]},
                   {**self.record, "command": [arg.replace("pebble-emery", "pebble-gabbro")
                                               for arg in self.record["command"]]}]
        for index, record in enumerate(records):
            with self.subTest(index=index), patch.object(Path, "is_symlink", return_value=False), \
                    patch.object(Path, "exists", return_value=True), \
                    patch.object(Path, "read_text", return_value=json.dumps(record)):
                if index == 0:
                    self.assertEqual(emulator.read_record(self.directory, "emery"), self.record)
                else:
                    with self.assertRaises(RuntimeError):
                        emulator.read_record(self.directory, "emery")

    def test_inspect_accepts_only_matching_pid_command_start_and_exclusive_flash(self):
        cases = [(self.identity, {321}, True), (None, set(), False),
                 ((self.identity[0], "different start time"), {321}, "reject"),
                 (("other command", self.identity[1]), {321}, "reject"),
                 (self.identity, {321, 999}, "reject"), (None, {999}, "reject")]
        for identity, active, expected in cases:
            with self.subTest(identity=identity, active=active), \
                    patch.object(emulator, "read_record", return_value=self.record), \
                    patch.object(emulator, "flash_processes", return_value=active), \
                    patch.object(emulator, "process_identity", return_value=identity):
                if expected == "reject":
                    with self.assertRaises(RuntimeError):
                        emulator.inspect(self.directory, "emery")
                else:
                    self.assertEqual(emulator.inspect(self.directory, "emery"), self.record if expected else None)

    def test_missing_record_never_adopts_an_unknown_process(self):
        with patch.object(emulator, "read_record", return_value=None), \
                patch.object(emulator, "flash_processes", return_value={321}), self.assertRaises(RuntimeError):
            emulator.inspect(self.directory, "emery")

    def test_process_identity_does_not_signal(self):
        with patch.object(emulator.subprocess, "run", side_effect=[
                MagicMock(returncode=0, stdout="command with spaces\n"),
                MagicMock(returncode=0, stdout="start time\n")]) as run:
            self.assertEqual(emulator.process_identity(321), ("command with spaces", "start time"))
            self.assertEqual(run.call_args_list[0].args[0], ["ps", "-ww", "-p", "321", "-o", "command="])
        with patch.object(emulator.subprocess, "run", return_value=MagicMock(returncode=1, stdout="")):
            self.assertIsNone(emulator.process_identity(321))
        with patch.object(emulator.subprocess, "run", return_value=MagicMock(returncode=2, stderr="denied")), \
                self.assertRaises(RuntimeError):
            emulator.process_identity(321)

    def test_flash_process_scan_includes_unrecorded_users(self):
        output = f" 321 qemu -drive file={self.directory / 'flash.bin'}\n 999 unrelated process\n"
        with patch.object(emulator.subprocess, "run", return_value=MagicMock(stdout=output)):
            self.assertEqual(emulator.flash_processes(self.directory / "flash.bin"), {321})

    def test_busy_port_refused_without_launch(self):
        with patch.object(emulator.socket, "socket") as socket:
            socket.return_value.__enter__.return_value.bind.side_effect = OSError("busy")
            with self.assertRaisesRegex(RuntimeError, "occupied"):
                emulator.require_free_port(12344)
        with patch.object(emulator, "inspect", return_value=None), \
                patch.object(emulator, "require_free_port", side_effect=RuntimeError("busy")), \
                patch.object(emulator, "sdk_paths") as sdk, patch.object(emulator.subprocess, "Popen") as popen:
            with self.assertRaises(RuntimeError):
                emulator.start(self.directory, "emery", 12344)
            sdk.assert_not_called()
            popen.assert_not_called()

    def test_start_is_idempotent_but_refuses_a_different_port(self):
        with patch.object(emulator, "inspect", return_value=self.record), \
                patch.object(emulator.subprocess, "Popen") as popen:
            self.assertEqual(emulator.start(self.directory, "emery", 12344), self.record)
            with self.assertRaises(RuntimeError):
                emulator.start(self.directory, "emery", 12345)
            popen.assert_not_called()

    def test_launch_arguments_and_environment_are_repo_scoped(self):
        for platform in ("emery", "gabbro"):
            directory = emulator.ROOT / "build/emulator" / platform
            micro = Path("/sdk/sdk-core/pebble") / platform / "qemu/qemu_micro_flash.bin"
            with self.subTest(platform=platform), \
                    patch.object(emulator, "inspect", return_value=None), \
                    patch.object(emulator, "require_free_port"), \
                    patch.object(emulator, "sdk_paths", return_value=(Path("/sdk/toolchain/bin/qemu-pebble"), micro, Path("/sdk/image.bz2"))), \
                    patch.object(emulator, "initialise_flash"), \
                    patch.object(emulator, "checked_file", side_effect=lambda folder, name: folder / name), \
                    patch.object(Path, "open"), patch.object(Path, "write_text") as write, \
                    patch.object(emulator.subprocess, "Popen") as popen, \
                    patch.object(emulator, "process_identity") as identity, \
                    patch.object(emulator.socket, "create_connection"), \
                    patch.dict(emulator.os.environ, {"DYLD_LIBRARY_PATH": "/existing"}), \
                    patch.object(emulator.time, "monotonic", side_effect=[0, 1]):
                popen.return_value.pid = 321
                popen.return_value.poll.return_value = None
                identity.side_effect = lambda pid: (" ".join(popen.call_args.args[0]), "start timestamp")
                result = emulator.start(directory, platform, emulator.PORTS[platform])
                command = popen.call_args.args[0]
                self.assertIn(f"if=mtd,format=raw,file={directory / 'flash.bin'}", command)
                self.assertIn(f"file:{directory / 'serial.log'}", command)
                self.assertIn(f"tcp:127.0.0.1:{emulator.PORTS[platform]},server=on,wait=off", command)
                self.assertEqual(popen.call_args.kwargs["env"]["DYLD_LIBRARY_PATH"], "/sdk/toolchain/lib:/existing")
                self.assertEqual(popen.call_args.kwargs["cwd"], emulator.ROOT)
                self.assertTrue(popen.call_args.kwargs["start_new_session"])
                self.assertEqual(json.loads(write.call_args.args[0])["command"], command)
                self.assertEqual(result["pid"], 321)

    def test_existing_flash_is_never_reinitialised_by_start(self):
        flash = MagicMock()
        flash.exists.return_value = True
        with patch.object(emulator, "checked_file", return_value=flash), patch.object(emulator.bz2, "open") as source:
            emulator.initialise_flash(self.directory, Path("/sdk/image.bz2"))
            source.assert_not_called()
            flash.unlink.assert_not_called()

    def test_failed_decompression_removes_only_our_partial_output(self):
        for created in (True, False):
            with self.subTest(created=created):
                flash = MagicMock()
                flash.exists.return_value = False
                if not created:
                    flash.open.side_effect = FileExistsError("another owner created flash")
                with patch.object(emulator, "checked_file", return_value=flash), \
                        patch.object(emulator.bz2, "open"), \
                        patch.object(emulator.shutil, "copyfileobj", side_effect=OSError("truncated image")):
                    with self.assertRaises(OSError):
                        emulator.initialise_flash(self.directory, Path("/sdk/image.bz2"))
                self.assertEqual(flash.unlink.call_count, int(created))

    def test_stop_checks_identity_again_before_signalling(self):
        with patch.object(emulator, "inspect", return_value=self.record), \
                patch.object(emulator, "process_identity", return_value=("reused pid", "later")), \
                patch.object(emulator.os, "kill") as kill, self.assertRaises(RuntimeError):
            emulator.stop(self.directory, "emery")
        kill.assert_not_called()

    def test_stop_signals_only_verified_pid_and_preserves_flash(self):
        with patch.object(emulator, "inspect", return_value=self.record), \
                patch.object(emulator, "process_identity", side_effect=[self.identity, None]), \
                patch.object(emulator.os, "kill") as kill, patch.object(emulator, "checked_file") as file:
            emulator.stop(self.directory, "emery")
            kill.assert_called_once_with(321, signal.SIGTERM)
            file.assert_called_once_with(self.directory, "process.json")

    def test_stop_timeout_does_not_force_kill(self):
        with patch.object(emulator, "inspect", return_value=self.record), \
                patch.object(emulator, "process_identity", return_value=self.identity), \
                patch.object(emulator.time, "monotonic", side_effect=[0, 11]), \
                patch.object(emulator.os, "kill") as kill, self.assertRaisesRegex(RuntimeError, "no forced kill"):
            emulator.stop(self.directory, "emery")
        kill.assert_called_once_with(321, signal.SIGTERM)

    def test_reset_aborts_before_touching_flash_when_stop_or_port_check_fails(self):
        for failed_step in ("stop", "require_free_port"):
            with self.subTest(failed_step=failed_step), patch.object(emulator, "stop"), \
                    patch.object(emulator, "inspect", return_value=None), \
                    patch.object(emulator, "require_free_port"), \
                    patch.object(emulator, failed_step, side_effect=RuntimeError("refused")), \
                    patch.object(emulator, "checked_file") as file:
                with self.assertRaises(RuntimeError):
                    emulator.reset(self.directory, "emery", 12344)
                file.assert_not_called()

    def test_explicit_reset_replaces_only_flash_and_process_record(self):
        flash = MagicMock()
        flash.exists.return_value = True
        backup = MagicMock()
        backup.exists.return_value = False
        record = MagicMock()
        with patch.object(emulator, "stop") as stop, patch.object(emulator, "inspect", return_value=None), \
                patch.object(emulator, "require_free_port") as port, \
                patch.object(emulator, "sdk_paths", return_value=(None, None, Path("/sdk/image.bz2"))), \
                patch.object(emulator, "checked_file", side_effect=[flash, backup, record]) as file, \
                patch.object(emulator.time, "time_ns", return_value=123456), \
                patch.object(emulator, "initialise_flash") as initialise:
            emulator.reset(self.directory, "emery", 12344)
            stop.assert_called_once_with(self.directory, "emery")
            port.assert_called_once_with(12344)
            self.assertEqual([call.args[1] for call in file.call_args_list],
                             ["flash.bin", "flash-before-reset-123456.bin", "process.json"])
            flash.rename.assert_called_once_with(backup)
            flash.unlink.assert_not_called()
            initialise.assert_called_once_with(self.directory, Path("/sdk/image.bz2"))

    def test_reset_refuses_to_overwrite_existing_backup(self):
        with patch.object(emulator, "stop"), patch.object(emulator, "inspect", return_value=None), \
                patch.object(emulator, "require_free_port"), \
                patch.object(emulator, "sdk_paths", return_value=(None, None, Path("/sdk/image.bz2"))), \
                patch.object(emulator, "checked_file") as file, \
                patch.object(emulator, "initialise_flash") as initialise:
            file.return_value.exists.return_value = True
            with self.assertRaisesRegex(RuntimeError, "backup already exists"):
                emulator.reset(self.directory, "emery", 12344)
            file.return_value.rename.assert_not_called()
            initialise.assert_not_called()


if __name__ == "__main__":
    unittest.main()
