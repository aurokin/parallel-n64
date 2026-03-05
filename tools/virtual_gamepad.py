#!/usr/bin/env python3
"""Create and drive a virtual gamepad via /dev/uinput.

Usage examples:
  # Terminal 1: start daemon
  python3 tools/virtual_gamepad.py daemon

  # Terminal 2: send inputs
  python3 tools/virtual_gamepad.py send tap start
  python3 tools/virtual_gamepad.py send tap a 120
  python3 tools/virtual_gamepad.py send pulse a 3 500 120
  python3 tools/virtual_gamepad.py send down left
  python3 tools/virtual_gamepad.py send up left

  # Stop daemon
  python3 tools/virtual_gamepad.py stop
"""

from __future__ import annotations

import argparse
import errno
import fcntl
import os
import shlex
import socket
import struct
import sys
import time
from typing import Dict, List, Tuple

DEFAULT_SOCKET = "/tmp/parallel-n64-vpad.sock"
UINPUT_PATH = "/dev/uinput"
ABS_CNT = 0x40

# Linux input event constants.
EV_SYN = 0x00
EV_KEY = 0x01
EV_ABS = 0x03

SYN_REPORT = 0

ABS_X = 0x00
ABS_Y = 0x01
ABS_Z = 0x02
ABS_RX = 0x03
ABS_RY = 0x04
ABS_RZ = 0x05
ABS_HAT0X = 0x10
ABS_HAT0Y = 0x11

BTN_SOUTH = 0x130  # A
BTN_EAST = 0x131   # B
BTN_NORTH = 0x133  # X
BTN_WEST = 0x134   # Y
BTN_TL = 0x136
BTN_TR = 0x137
BTN_TL2 = 0x138
BTN_TR2 = 0x139
BTN_SELECT = 0x13A
BTN_START = 0x13B
BTN_MODE = 0x13C
BTN_THUMBL = 0x13D
BTN_THUMBR = 0x13E

BTN_DPAD_UP = 0x220
BTN_DPAD_DOWN = 0x221
BTN_DPAD_LEFT = 0x222
BTN_DPAD_RIGHT = 0x223

BUS_USB = 0x03

# ioctl helpers from linux/ioctl.h
_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
_IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
_IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS

_IOC_NONE = 0
_IOC_WRITE = 1



def _IOC(direction: int, io_type: str, number: int, size: int) -> int:
    return (
        (direction << _IOC_DIRSHIFT)
        | (ord(io_type) << _IOC_TYPESHIFT)
        | (number << _IOC_NRSHIFT)
        | (size << _IOC_SIZESHIFT)
    )



def _IO(io_type: str, number: int) -> int:
    return _IOC(_IOC_NONE, io_type, number, 0)



def _IOW(io_type: str, number: int, size: int) -> int:
    return _IOC(_IOC_WRITE, io_type, number, size)


UI_SET_EVBIT = _IOW("U", 100, struct.calcsize("i"))
UI_SET_KEYBIT = _IOW("U", 101, struct.calcsize("i"))
UI_SET_ABSBIT = _IOW("U", 103, struct.calcsize("i"))
UI_DEV_CREATE = _IO("U", 1)
UI_DEV_DESTROY = _IO("U", 2)

EVENT_STRUCT = struct.Struct("llHHi")

BUTTONS: Dict[str, int] = {
    "a": BTN_SOUTH,
    "b": BTN_EAST,
    "x": BTN_NORTH,
    "y": BTN_WEST,
    "lb": BTN_TL,
    "rb": BTN_TR,
    "lt": BTN_TL2,
    "rt": BTN_TR2,
    "select": BTN_SELECT,
    "back": BTN_SELECT,
    "start": BTN_START,
    "mode": BTN_MODE,
    "guide": BTN_MODE,
    "l3": BTN_THUMBL,
    "r3": BTN_THUMBR,
    "up": BTN_DPAD_UP,
    "down": BTN_DPAD_DOWN,
    "left": BTN_DPAD_LEFT,
    "right": BTN_DPAD_RIGHT,
}

ABS_CONFIG: Dict[int, Tuple[int, int, int]] = {
    ABS_X: (-32768, 32767, 128),
    ABS_Y: (-32768, 32767, 128),
    ABS_RX: (-32768, 32767, 128),
    ABS_RY: (-32768, 32767, 128),
    ABS_Z: (0, 255, 0),
    ABS_RZ: (0, 255, 0),
    ABS_HAT0X: (-1, 1, 0),
    ABS_HAT0Y: (-1, 1, 0),
}


class VirtualGamepad:
    def __init__(self, name: str = "parallel-n64 Virtual Pad") -> None:
        self.name = name
        self.fd: int | None = None

    def create(self) -> None:
        if self.fd is not None:
            return

        fd = os.open(UINPUT_PATH, os.O_WRONLY | os.O_NONBLOCK)
        try:
            fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
            fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
            fcntl.ioctl(fd, UI_SET_EVBIT, EV_SYN)

            for code in BUTTONS.values():
                fcntl.ioctl(fd, UI_SET_KEYBIT, code)

            for code in ABS_CONFIG:
                fcntl.ioctl(fd, UI_SET_ABSBIT, code)

            user_dev = self._build_user_dev_bytes()
            os.write(fd, user_dev)
            fcntl.ioctl(fd, UI_DEV_CREATE)

            self.fd = fd
            # Give userspace (SDL/RetroArch) time to enumerate the new input device.
            time.sleep(0.35)
        except Exception:
            os.close(fd)
            raise

    def destroy(self) -> None:
        if self.fd is None:
            return

        try:
            fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        finally:
            os.close(self.fd)
            self.fd = None

    def _build_user_dev_bytes(self) -> bytes:
        name_bytes = self.name.encode("utf-8")[:79]
        name_field = name_bytes + b"\x00" * (80 - len(name_bytes))

        absmax = [0] * ABS_CNT
        absmin = [0] * ABS_CNT
        absfuzz = [0] * ABS_CNT
        absflat = [0] * ABS_CNT

        for code, (mn, mx, flat) in ABS_CONFIG.items():
            if 0 <= code < ABS_CNT:
                absmin[code] = mn
                absmax[code] = mx
                absflat[code] = flat

        header = struct.pack("<80sHHHHI", name_field, BUS_USB, 0x045E, 0x028E, 1, 0)
        arrays = struct.pack("<{}i".format(ABS_CNT * 4), *(absmax + absmin + absfuzz + absflat))
        return header + arrays

    def _emit(self, ev_type: int, code: int, value: int) -> None:
        if self.fd is None:
            raise RuntimeError("virtual gamepad is not active")
        now = time.time()
        sec = int(now)
        usec = int((now - sec) * 1_000_000)
        os.write(self.fd, EVENT_STRUCT.pack(sec, usec, ev_type, code, value))

    def _syn(self) -> None:
        self._emit(EV_SYN, SYN_REPORT, 0)

    def down(self, button: str) -> None:
        code = self._button_code(button)
        self._emit(EV_KEY, code, 1)
        self._syn()

    def up(self, button: str) -> None:
        code = self._button_code(button)
        self._emit(EV_KEY, code, 0)
        self._syn()

    def tap(self, button: str, hold_ms: int = 120) -> None:
        self.down(button)
        time.sleep(max(1, hold_ms) / 1000.0)
        self.up(button)

    def pulse(self, button: str, count: int, interval_ms: int, hold_ms: int) -> None:
        for i in range(count):
            self.tap(button, hold_ms=hold_ms)
            if i + 1 < count:
                time.sleep(max(0, interval_ms) / 1000.0)

    @staticmethod
    def _button_code(name: str) -> int:
        key = name.lower()
        if key not in BUTTONS:
            raise ValueError(f"unknown button '{name}'. supported: {', '.join(sorted(BUTTONS))}")
        return BUTTONS[key]


class Daemon:
    def __init__(self, socket_path: str) -> None:
        self.socket_path = socket_path

    def run(self) -> int:
        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)

        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(self.socket_path)
        server.listen(8)
        os.chmod(self.socket_path, 0o600)

        pad = VirtualGamepad()
        try:
            pad.create()
            print(f"virtual-pad: ready (socket={self.socket_path}, device='{pad.name}')")
            sys.stdout.flush()

            while True:
                conn, _ = server.accept()
                with conn:
                    payload = self._recv_all(conn)
                    cmd = payload.decode("utf-8", errors="replace").strip()
                    if not cmd:
                        conn.sendall(b"ERR empty command\n")
                        continue

                    ok, response, should_quit = self._handle_command(pad, cmd)
                    prefix = "OK" if ok else "ERR"
                    conn.sendall(f"{prefix} {response}\n".encode("utf-8"))
                    if should_quit:
                        return 0
        finally:
            pad.destroy()
            server.close()
            try:
                os.unlink(self.socket_path)
            except OSError:
                pass

    @staticmethod
    def _recv_all(conn: socket.socket) -> bytes:
        chunks: List[bytes] = []
        while True:
            data = conn.recv(4096)
            if not data:
                break
            chunks.append(data)
        return b"".join(chunks)

    @staticmethod
    def _handle_command(pad: VirtualGamepad, cmdline: str) -> Tuple[bool, str, bool]:
        try:
            tokens = shlex.split(cmdline)
        except ValueError as e:
            return False, f"parse error: {e}", False

        if not tokens:
            return False, "empty command", False

        cmd = tokens[0].lower()

        try:
            if cmd in {"quit", "stop", "exit"}:
                return True, "stopping daemon", True
            if cmd == "help":
                return True, (
                    "commands: tap <btn> [hold_ms], down <btn>, up <btn>, "
                    "pulse <btn> <count> <interval_ms> [hold_ms], quit"
                ), False
            if cmd == "tap":
                if len(tokens) not in {2, 3}:
                    return False, "usage: tap <button> [hold_ms]", False
                hold_ms = int(tokens[2]) if len(tokens) == 3 else 120
                if hold_ms <= 0:
                    return False, "hold_ms must be > 0", False
                pad.tap(tokens[1], hold_ms=hold_ms)
                return True, f"tap {tokens[1]} hold={hold_ms}ms", False
            if cmd == "down":
                if len(tokens) != 2:
                    return False, "usage: down <button>", False
                pad.down(tokens[1])
                return True, f"down {tokens[1]}", False
            if cmd == "up":
                if len(tokens) != 2:
                    return False, "usage: up <button>", False
                pad.up(tokens[1])
                return True, f"up {tokens[1]}", False
            if cmd == "pulse":
                if len(tokens) not in {4, 5}:
                    return False, "usage: pulse <button> <count> <interval_ms> [hold_ms]", False
                count = int(tokens[2])
                interval_ms = int(tokens[3])
                hold_ms = int(tokens[4]) if len(tokens) == 5 else 120
                if count <= 0:
                    return False, "count must be > 0", False
                if interval_ms < 0:
                    return False, "interval_ms must be >= 0", False
                if hold_ms <= 0:
                    return False, "hold_ms must be > 0", False
                pad.pulse(tokens[1], count=count, interval_ms=interval_ms, hold_ms=hold_ms)
                return True, (
                    f"pulse {tokens[1]} count={count} interval={interval_ms}ms hold={hold_ms}ms"
                ), False

            return False, f"unknown command '{cmd}'", False
        except ValueError as e:
            return False, str(e), False
        except Exception as e:  # pragma: no cover - runtime safeguard
            return False, f"runtime error: {type(e).__name__}: {e}", False


def send_command(socket_path: str, command: str) -> int:
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        client.connect(socket_path)
        client.sendall((command + "\n").encode("utf-8"))
        client.shutdown(socket.SHUT_WR)

        response = b""
        while True:
            data = client.recv(4096)
            if not data:
                break
            response += data

        text = response.decode("utf-8", errors="replace").strip()
        if not text:
            print("ERR empty response from daemon", file=sys.stderr)
            return 1

        print(text)
        return 0 if text.startswith("OK ") else 1
    except FileNotFoundError:
        print(f"ERR socket not found: {socket_path}", file=sys.stderr)
        return 1
    except OSError as e:
        if e.errno == errno.ENOENT:
            print(f"ERR socket not found: {socket_path}", file=sys.stderr)
        else:
            print(f"ERR failed to send command: {e}", file=sys.stderr)
        return 1
    finally:
        client.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Virtual gamepad daemon + command client.")
    sub = parser.add_subparsers(dest="mode", required=True)

    daemon = sub.add_parser("daemon", help="Start the virtual gamepad daemon.")
    daemon.add_argument("--socket", default=DEFAULT_SOCKET, help=f"UNIX socket path (default: {DEFAULT_SOCKET})")

    send = sub.add_parser("send", help="Send a command to a running daemon.")
    send.add_argument("--socket", default=DEFAULT_SOCKET, help=f"UNIX socket path (default: {DEFAULT_SOCKET})")
    send.add_argument("command", nargs=argparse.REMAINDER, help="Command tokens, e.g. tap start 120")

    stop = sub.add_parser("stop", help="Stop a running daemon.")
    stop.add_argument("--socket", default=DEFAULT_SOCKET, help=f"UNIX socket path (default: {DEFAULT_SOCKET})")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.mode == "daemon":
        return Daemon(args.socket).run()

    if args.mode == "stop":
        return send_command(args.socket, "quit")

    # send mode
    command_parts = args.command
    if command_parts and command_parts[0] == "--":
        command_parts = command_parts[1:]
    if not command_parts:
        print("ERR no command provided. examples: 'tap start', 'pulse a 3 500 120'", file=sys.stderr)
        return 1

    command = " ".join(command_parts)
    return send_command(args.socket, command)


if __name__ == "__main__":
    sys.exit(main())
