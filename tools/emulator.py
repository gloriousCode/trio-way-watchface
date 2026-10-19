#!/usr/bin/env python3
"""Manage only this checkout's emery/gabbro audit emulators (modern SDK/QEMU).

Flash, process records and logs live in build/emulator/<platform>. Reset is an
explicit stop plus flash replacement; starting never resets existing flash.
"""

import argparse
import bz2
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
PORTS = {"emery": 12344, "gabbro": 12345}


def data_directory(platform):
    """Reject unknown platforms and symlinks escaping the checkout."""
    if platform not in PORTS:
        raise ValueError("Unsupported platform")
    directory = ROOT / "build" / "emulator" / platform
    if directory.resolve() != directory:
        raise RuntimeError(f"Refusing redirected emulator directory: {directory}")
    return directory


def checked_file(directory, name):
    """Refuse symlinked emulator state, flash or log files."""
    path = directory / name
    if path.is_symlink():
        raise RuntimeError(f"Refusing symlink: {path}")
    return path


@contextmanager
def platform_lock(directory):
    """Serialise lifecycle operations for one platform."""
    directory.mkdir(parents=True, exist_ok=True)
    with checked_file(directory, "lifecycle.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def process_identity(pid):
    """Read the exact visible argv and start timestamp without signalling."""
    values = []
    for field in ("command", "lstart"):
        result = subprocess.run(["ps", "-ww", "-p", str(pid), "-o", f"{field}="],
                                capture_output=True, text=True, check=False)
        if result.returncode == 1 and not result.stdout.strip():
            return None
        if result.returncode:
            raise RuntimeError(f"Cannot inspect PID {pid}: {result.stderr.strip()}")
        values.append(result.stdout.strip())
    return tuple(values)


def flash_processes(flash):
    """Find even unrecorded processes referencing our flash before changing it."""
    result = subprocess.run(["ps", "-ww", "-axo", "pid=,command="],
                            capture_output=True, text=True, check=True)
    pids = set()
    for line in result.stdout.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) == 2 and str(flash) in parts[1]:
            pids.add(int(parts[0]))
    return pids


def read_record(directory, platform):
    """Validate stored ownership before trusting its PID or command."""
    path = checked_file(directory, "process.json")
    if not path.exists():
        return None
    record = json.loads(path.read_text())
    if not isinstance(record, dict):
        raise RuntimeError("Invalid emulator ownership record; refusing lifecycle changes")
    command = record.get("command")
    flash_arg = f"if=mtd,format=raw,file={directory / 'flash.bin'}"
    if (type(record.get("pid")) is not int or record["pid"] <= 1 or
            type(record.get("port")) is not int or not 1024 <= record["port"] <= 65535 or
            not isinstance(record.get("started"), str) or not record["started"] or
            not isinstance(command, list) or not command or
            not all(isinstance(arg, str) for arg in command) or
            Path(command[0]).name != "qemu-pebble" or
            not any(command[i:i + 2] == ["-drive", flash_arg] for i in range(len(command))) or
            not any(command[i:i + 2] == ["-machine", f"pebble-{platform}"]
                    for i in range(len(command)))):
        raise RuntimeError("Invalid emulator ownership record; refusing lifecycle changes")
    return record


def inspect(directory, platform):
    """Return a verified running record, or None; unknown ownership is an error."""
    record = read_record(directory, platform)
    active = flash_processes(directory / "flash.bin")
    if record is None:
        if active:
            raise RuntimeError(f"Unrecorded process uses this flash: {sorted(active)}")
        return None
    identity = process_identity(record["pid"])
    if identity is None:
        if active:
            raise RuntimeError(f"Unknown process uses this flash: {sorted(active)}")
        return None
    if identity != (" ".join(record["command"]), record["started"]) or active != {record["pid"]}:
        raise RuntimeError("PID identity or flash ownership changed; refusing to signal or reset")
    return record


def require_free_port(port):
    """Refuse a busy transport port without disconnecting its owner."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        try:
            listener.bind(("127.0.0.1", port))
        except OSError as error:
            raise RuntimeError(f"Port {port} is occupied or unavailable") from error


def sdk_paths(platform):
    """Resolve the configured modern SDK without modifying SDK-wide state."""
    sdk = Path(os.environ.get("PEBBLE_SDK_ROOT",
               str(Path.home() / "Library/Application Support/Pebble SDK/SDKs/current"))).expanduser().resolve()
    executable = Path(os.environ.get("PEBBLE_QEMU_PATH", str(sdk / "toolchain/bin/qemu-pebble"))).expanduser().resolve()
    firmware = sdk / "sdk-core/pebble" / platform / "qemu"
    micro = firmware / "qemu_micro_flash.bin"
    compressed = firmware / "qemu_spi_flash.bin.bz2"
    for path in (executable, micro, compressed):
        if not path.is_file():
            raise RuntimeError(f"Missing SDK emulator file: {path}")
    return executable, micro, compressed


def initialise_flash(directory, compressed):
    """Decompress only absent flash; remove partial output if decompression fails."""
    flash = checked_file(directory, "flash.bin")
    if flash.exists():
        return
    created = False
    try:
        with bz2.open(compressed, "rb") as source:
            with flash.open("xb") as destination:
                created = True
                shutil.copyfileobj(source, destination)
    except Exception:
        if created:
            flash.unlink()
        raise


def start(directory, platform, port):
    """Launch a headless emulator, preserving existing flash and append-only logs."""
    current = inspect(directory, platform)
    if current:
        if current["port"] != port:
            raise RuntimeError(f"Already running on port {current['port']}; stop explicitly first")
        return current
    require_free_port(port)
    executable, micro, compressed = sdk_paths(platform)
    initialise_flash(directory, compressed)
    command = [str(executable), "-machine", f"pebble-{platform}", "-cpu", "cortex-m33",
               "-rtc", "base=localtime", "-serial", "null", "-serial",
               f"tcp:127.0.0.1:{port},server=on,wait=off", "-serial",
               f"file:{checked_file(directory, 'serial.log')}",
               "-kernel", str(micro), "-drive", f"if=mtd,format=raw,file={directory / 'flash.bin'}",
               "-display", "none", "-monitor", "none"]
    if platform == "emery":
        command += ["-audio", "driver=none,id=audio0"]
    environment = os.environ.copy()
    library_path = str(micro.parents[4] / "toolchain/lib")
    if environment.get("DYLD_LIBRARY_PATH"):
        library_path += os.pathsep + environment["DYLD_LIBRARY_PATH"]
    environment["DYLD_LIBRARY_PATH"] = library_path
    with checked_file(directory, "emulator.log").open("ab") as log:
        process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True, env=environment)
    identity = process_identity(process.pid)
    if identity is None or identity[0] != " ".join(command):
        raise RuntimeError("Emulator exited or could not be identified; inspect emulator.log")
    record = {"pid": process.pid, "port": port, "command": command, "started": identity[1]}
    checked_file(directory, "process.json").write_text(json.dumps(record, indent=2) + "\n")
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("Emulator exited during startup; inspect emulator.log (flash retained)")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return record
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("Startup transport timeout; state retained. Inspect status/logs; stop explicitly")


def stop(directory, platform):
    """Send SIGTERM only after matching PID, timestamp and exact flash command."""
    current = inspect(directory, platform)
    if current is None:
        return
    # Recheck immediately before signalling, including PID reuse protection.
    if process_identity(current["pid"]) != (" ".join(current["command"]), current["started"]):
        raise RuntimeError("Process identity changed before stop; refusing to signal")
    os.kill(current["pid"], signal.SIGTERM)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        identity = process_identity(current["pid"])
        if identity is None:
            checked_file(directory, "process.json").unlink(missing_ok=True)
            return
        if identity != (" ".join(current["command"]), current["started"]):
            raise RuntimeError("PID changed while stopping; no further signals sent")
        time.sleep(0.1)
    raise RuntimeError("Emulator did not exit after SIGTERM; no forced kill or flash reset attempted")


def reset(directory, platform, port):
    """Explicitly stop, verify quiescence and retain old flash before replacing it."""
    stop(directory, platform)
    if inspect(directory, platform) is not None:
        raise RuntimeError("Emulator still running; refusing reset")
    require_free_port(port)
    _, _, compressed = sdk_paths(platform)
    flash = checked_file(directory, "flash.bin")
    if flash.exists():
        backup = checked_file(directory, f"flash-before-reset-{time.time_ns()}.bin")
        if backup.exists():
            raise RuntimeError(f"Reset backup already exists: {backup}")
        flash.rename(backup)
    checked_file(directory, "process.json").unlink(missing_ok=True)
    initialise_flash(directory, compressed)


def main():
    """Dispatch explicit lifecycle actions; reset leaves the emulator stopped."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "status", "stop", "reset"))
    parser.add_argument("platform", choices=PORTS)
    parser.add_argument("--port", type=int)
    args = parser.parse_args()
    port = args.port if args.port is not None else PORTS[args.platform]
    if not 1024 <= port <= 65535:
        parser.error("port must be between 1024 and 65535")
    try:
        directory = data_directory(args.platform)
        with platform_lock(directory):
            if args.action == "start":
                start(directory, args.platform, port)
            elif args.action == "stop":
                stop(directory, args.platform)
            elif args.action == "reset":
                reset(directory, args.platform, port)
            record = inspect(directory, args.platform)
            print(json.dumps({"platform": args.platform, "running": record is not None,
                              "pid": record["pid"] if record else None,
                              "port": record["port"] if record else port, "directory": str(directory)}))
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
