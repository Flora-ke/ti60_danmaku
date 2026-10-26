#!/usr/bin/env python3
"""115200 8N1 keyboard state sender. Optional PC-only dependency: pyserial.

Windows: py uart_bridge.py --port COM6
Linux:   python3 uart_bridge.py --port /dev/ttyUSB0
No serial write occurs with --self-test. Never flashes a board.
Windows defaults to held-key polling while the startup terminal has focus.
--terminal uses terminal-repeat leases instead (the default on Linux).
"""
import argparse
import os
import select
import sys
import time

LEFT, RIGHT, UP, DOWN, FIRE, START, PAUSE, DEMO, LEVEL, DIFFICULTY = (1 << n for n in range(10))
BACKEND_CPU, BACKEND_G2D, BACKEND_COMPARE = (1 << n for n in range(10, 13))
BACKEND_KEYS = (BACKEND_CPU, BACKEND_G2D, BACKEND_COMPARE)
INPUT_MASK = 0x1fff
KEY_MAPPING = {"a": LEFT, "d": RIGHT, "w": UP, "s": DOWN, "z": FIRE,
               "r": START, "\r": START, "\n": START, "p": PAUSE, "m": DEMO, "k": LEVEL,
               "n": DIFFICULTY, "1": BACKEND_CPU, "2": BACKEND_G2D,
               "3": BACKEND_COMPARE}

# Explicit Win32 virtual keys. Main-row and numeric-pad selections share bits.
WINDOWS_KEY_MAPPING = {
    0x25: LEFT, 0x41: LEFT, 0x27: RIGHT, 0x44: RIGHT,
    0x26: UP, 0x57: UP, 0x28: DOWN, 0x53: DOWN,
    0x5a: FIRE, 0x20: FIRE, 0x0d: START, 0x52: START,
    0x50: PAUSE, 0x4d: DEMO, 0x4b: LEVEL, 0x4e: DIFFICULTY,
    0x31: BACKEND_CPU, 0x61: BACKEND_CPU,
    0x32: BACKEND_G2D, 0x62: BACKEND_G2D,
    0x33: BACKEND_COMPARE, 0x63: BACKEND_COMPARE,
}
WINDOWS_QUIT_KEYS = frozenset((0x1b, 0x51))  # Escape, Q
WINDOWS_CTRL_KEYS = frozenset((0x11, 0xa2, 0xa3))
WINDOWS_POLL_KEYS = frozenset(WINDOWS_KEY_MAPPING) | WINDOWS_QUIT_KEYS | WINDOWS_CTRL_KEYS | {0x43}


def windows_key_snapshot(down, focused):
    """Pure physical-key mapping; no leases, WinAPI, serial or retained state.

    Opposite directions and simultaneous 1/2/3 are transmitted faithfully;
    the shared platform/game input rules resolve those combinations.
    """
    if not focused:
        return 0, False
    down = frozenset(down)
    quit_requested = bool(down & WINDOWS_QUIT_KEYS) or (
        bool(down & WINDOWS_CTRL_KEYS) and 0x43 in down)
    if quit_requested:
        return 0, True
    keys = 0
    for virtual_key, bit in WINDOWS_KEY_MAPPING.items():
        if virtual_key in down:
            keys |= bit
    return keys, False


class WindowsKeyboard:
    """Poll only while the foreground terminal saved at startup is focused.

    Start this controller from the active terminal. No global hook is installed;
    unfocused samples do not query key states and immediately send zero.
    """
    def __init__(self):
        import ctypes
        from ctypes import wintypes
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.user32.GetForegroundWindow.argtypes = []
        self.user32.GetForegroundWindow.restype = wintypes.HWND
        self.user32.GetAsyncKeyState.argtypes = [ctypes.c_int]
        self.user32.GetAsyncKeyState.restype = ctypes.c_short
        self.window = self.user32.GetForegroundWindow()
        if not self.window:
            raise OSError("no foreground terminal window; run from an active terminal or use --terminal")

    def read(self):
        if self.user32.GetForegroundWindow() != self.window:
            return windows_key_snapshot((), False)
        # Consume terminal input records as well, so game keys do not remain
        # queued for the parent shell after this controller exits. Their repeat
        # events do not drive the physical snapshot below.
        import msvcrt
        while msvcrt.kbhit():
            msvcrt.getwch()
        down = {key for key in WINDOWS_POLL_KEYS
                if self.user32.GetAsyncKeyState(key) & 0x8000}
        # Focus can change while sampling; discard that entire snapshot.
        focused = self.user32.GetForegroundWindow() == self.window
        return windows_key_snapshot(down, focused)


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
    return crc


def packet(keys, sequence):
    if keys & ~INPUT_MASK or keys < 0:
        raise ValueError("keys must be a 13-bit game/platform input bitmap")
    payload = bytes((1, 3, sequence & 255, keys & 255, keys >> 8))
    return b"\xa5\x5a" + payload + bytes((crc8(payload),))


def write_packet(serial_port, keys, sequence):
    """Require the complete existing 8-byte snapshot, including release frames."""
    frame = packet(keys, sequence)
    count = serial_port.write(frame)
    if count != len(frame):
        raise OSError("short UART write: %r of %d bytes" % (count, len(frame)))


class Console:
    """Terminal key repeats keep movement pressed; 180ms silence releases it.

    Terminals have no key-up event. This is a convenience controller, not a
    physical keyboard driver. The serial protocol supports true full snapshots.
    """
    def __enter__(self):
        self.saved = None
        self.pending = ""
        self.pending_since = 0.0
        if os.name != "nt":
            import termios
            import tty
            self.saved = termios.tcgetattr(sys.stdin.fileno())
            tty.setcbreak(sys.stdin.fileno())
        return self

    def read(self):
        chars = ""
        if os.name == "nt":
            import msvcrt
            while msvcrt.kbhit():
                c = msvcrt.getwch()
                if c in ("\x00", "\xe0"):
                    c = {"K": "a", "M": "d", "H": "w", "P": "s"}.get(msvcrt.getwch(), "")
                chars += c
        else:
            while select.select([sys.stdin], [], [], 0)[0]:
                part = os.read(sys.stdin.fileno(), 32)
                if not part:
                    raise EOFError()
                chars += part.decode("ascii", "ignore")
            # Escape sequences may arrive across two reads. Keep partial ones.
            self.pending += chars
            for arrow, key in (("A", "w"), ("B", "s"), ("C", "d"), ("D", "a")):
                self.pending = self.pending.replace("\x1b[" + arrow, key)
            tail = ""
            for partial in ("\x1b[", "\x1b"):
                if self.pending.endswith(partial):
                    if not chars and time.monotonic() - self.pending_since >= 0.06:
                        # A standalone Esc has no following arrow bytes.
                        break
                    self.pending, tail = self.pending[:-len(partial)], partial
                    if chars:
                        self.pending_since = time.monotonic()
                    break
            chars, self.pending = self.pending, tail
        return chars.lower()

    def __exit__(self, *_):
        if self.saved is not None:
            import termios
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self.saved)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="COM6 or /dev/ttyUSB0")
    parser.add_argument("--keys", type=lambda v: int(v, 0), help="constant bitmap, e.g. 0x10 (FIRE)")
    parser.add_argument("--duration", type=float, default=0, help="seconds; 0 runs until Q/Ctrl-C")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--terminal", action="store_true",
                        help="use terminal repeats/180ms leases instead of Windows held-key polling")
    args = parser.parse_args()
    if args.self_test:
        assert crc8(b"123456789") == 0xf4
        for keys in (0, 1, 0x10, 0x100, 0x200, 0x3ff, BACKEND_CPU, BACKEND_G2D,
                     BACKEND_COMPARE, INPUT_MASK):
            frame = packet(keys, 300)
            assert len(frame) == 8 and frame[:5] == b"\xa5\x5a\x01\x03\x2c"
            assert frame[5] | frame[6] << 8 == keys
            assert crc8(frame[2:-1]) == frame[-1]
        assert [KEY_MAPPING[c] for c in "123"] == list(BACKEND_KEYS)
        for virtual_key, bit in WINDOWS_KEY_MAPPING.items():
            assert windows_key_snapshot((virtual_key,), True) == (bit, False)
        assert windows_key_snapshot(WINDOWS_KEY_MAPPING, True) == (INPUT_MASK, False)
        assert windows_key_snapshot((0x25, 0x26, 0x20, 0x31, 0x63), True) == (
            LEFT | UP | FIRE | BACKEND_CPU | BACKEND_COMPARE, False)
        # Key-up is reflected by the next sample; there is no repeat timeout.
        assert windows_key_snapshot((0x25, 0x5a), True) == (LEFT | FIRE, False)
        assert windows_key_snapshot((0x5a,), True) == (FIRE, False)
        assert windows_key_snapshot((), True) == (0, False)
        assert windows_key_snapshot((0x31, 0x32, 0x33), True) == (
            BACKEND_CPU | BACKEND_G2D | BACKEND_COMPARE, False)
        assert windows_key_snapshot((0x61, 0x62, 0x63), True) == (
            BACKEND_CPU | BACKEND_G2D | BACKEND_COMPARE, False)
        assert windows_key_snapshot(WINDOWS_POLL_KEYS, False) == (0, False)
        assert windows_key_snapshot((0x25, 0x11, 0x43), True) == (0, True)
        for key in WINDOWS_QUIT_KEYS:
            assert windows_key_snapshot((key, 0x5a), True) == (0, True)
        assert windows_key_snapshot((0x43,), True) == (0, False)

        class FakeSerial:
            def __init__(self, result=8):
                self.result, self.frames = result, []

            def write(self, frame):
                self.frames.append(frame)
                return self.result

        fake = FakeSerial()
        write_packet(fake, FIRE | BACKEND_CPU, 255)
        write_packet(fake, 0, 0)
        assert fake.frames == [packet(FIRE | BACKEND_CPU, 255), packet(0, 0)]
        for count in (0, 7, None):
            try:
                write_packet(FakeSerial(count), 0, 0)
            except OSError:
                pass
            else:
                raise AssertionError("short UART write accepted")
        for bad in (-1, 0x2000, 0x8000, 0x10000):
            try:
                packet(bad, 0)
            except ValueError:
                pass
            else:
                raise AssertionError("out-of-range bitmap accepted")
        print("UART protocol/Windows snapshot/short-write self-test passed; zero packet:", packet(0, 0).hex(" "))
        return
    if not args.port:
        parser.error("--port is required")
    if args.duration < 0:
        parser.error("--duration must be >= 0")
    if args.keys is not None:
        packet(args.keys, 0)
    elif not sys.stdin.isatty():
        parser.error("interactive mode needs a terminal; use --keys for scripted operation")
    # Capture the foreground terminal before dependency import or serial open.
    # --self-test returns above, without calling WinAPI or touching a port.
    keyboard = WindowsKeyboard() if os.name == "nt" and not args.terminal and args.keys is None else None
    try:
        import serial
    except ImportError:
        parser.error("install the PC tool dependency first: python -m pip install pyserial")
    if keyboard:
        print("Windows physical keys: WASD/arrows move; hold Z/Space fires; Enter/R starts/restarts.")
        print("Only the startup terminal receives controls; losing its focus sends all keys UP.")
    else:
        print("Terminal repeats: WASD/arrows move; Z fires; Space toggles FIRE; Enter/R starts/restarts.")
    print("P pause; M auto demo; K density; N beginner/standard (title/end); Q/Esc/Ctrl-C quits. Heartbeat: 30Hz.")
    print("1 CPU; 2 G2D accelerator; 3 comparison (board selects after the current frame).")
    serial_port = serial.Serial(args.port, 115200, timeout=0, write_timeout=0.2)
    sequence, held_fire, expires = 0, False, {}
    started = time.monotonic()
    mapping = KEY_MAPPING

    def send(keys):
        nonlocal sequence
        write_packet(serial_port, keys, sequence)
        sequence = (sequence + 1) & 255
        incoming = serial_port.read(serial_port.in_waiting)
        if incoming:
            sys.stdout.write(incoming.decode("ascii", "replace"))
            sys.stdout.flush()

    release_error = None
    try:
        if args.keys is not None:
            while not args.duration or time.monotonic() - started < args.duration:
                send(args.keys)
                time.sleep(1 / 30)
        elif keyboard:
            while not args.duration or time.monotonic() - started < args.duration:
                keys, quit_requested = keyboard.read()
                if quit_requested:
                    break
                send(keys)
                time.sleep(1 / 30)
        else:
            with Console() as console:
                while not args.duration or time.monotonic() - started < args.duration:
                    now = time.monotonic()
                    quit_requested = False
                    for char in console.read():
                        if char in ("q", "\x1b", "\x03"):
                            quit_requested = True
                            break
                        if char == " ":
                            held_fire = not held_fire
                        if char in mapping:
                            if mapping[char] in BACKEND_KEYS:
                                # Terminal repeat leases overlap for 180ms. A new
                                # selection replaces the old one immediately.
                                for bit in BACKEND_KEYS:
                                    expires.pop(bit, None)
                            expires[mapping[char]] = now + 0.18
                    if quit_requested:
                        break
                    keys = FIRE if held_fire else 0
                    for bit, until in expires.items():
                        if now < until:
                            keys |= bit
                    send(keys)
                    time.sleep(1 / 30)
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        try:
            write_packet(serial_port, 0, sequence)
        except OSError as error:
            # Best effort: show release failure without hiding the main error.
            print("UART final all-keys-UP failed:", error, file=sys.stderr)
            release_error = error
        finally:
            try:
                serial_port.close()
            except OSError as error:
                print("UART close failed:", error, file=sys.stderr)
                release_error = error
    return 1 if release_error else 0


if __name__ == "__main__":
    raise SystemExit(main())
