#!/usr/bin/env python3
"""115200 8N1 keyboard state sender. Optional PC-only dependency: pyserial.

Windows: py uart_bridge.py --port COM6
Linux:   python3 uart_bridge.py --port /dev/ttyUSB0
No serial write occurs with --self-test. Never flashes a board.
"""
import argparse
import os
import select
import sys
import time

LEFT, RIGHT, UP, DOWN, FIRE, START, PAUSE, DEMO, LEVEL = (1 << n for n in range(9))


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
    return crc


def packet(keys, sequence):
    if keys & ~0x1ff or keys < 0:
        raise ValueError("keys must be a 9-bit game input bitmap")
    payload = bytes((1, 3, sequence & 255, keys & 255, keys >> 8))
    return b"\xa5\x5a" + payload + bytes((crc8(payload),))


class Console:
    """Terminal key repeats keep movement pressed; 180ms silence releases it.

    Terminals have no key-up event. This is a convenience controller, not a
    physical keyboard driver. The serial protocol supports true full snapshots.
    """
    def __enter__(self):
        self.saved = None
        self.pending = ""
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
                    self.pending, tail = self.pending[:-len(partial)], partial
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
    args = parser.parse_args()
    if args.self_test:
        assert crc8(b"123456789") == 0xf4
        for keys in (0, 1, 0x10, 0x100, 0x1ff):
            frame = packet(keys, 300)
            assert len(frame) == 8 and frame[:5] == b"\xa5\x5a\x01\x03\x2c"
            assert frame[5] | frame[6] << 8 == keys
            assert crc8(frame[2:-1]) == frame[-1]
        print("UART protocol self-test passed; zero packet:", packet(0, 0).hex(" "))
        return
    if not args.port:
        parser.error("--port is required")
    if args.duration < 0:
        parser.error("--duration must be >= 0")
    if args.keys is not None:
        packet(args.keys, 0)
    elif not sys.stdin.isatty():
        parser.error("interactive mode needs a terminal; use --keys for scripted operation")
    try:
        import serial
    except ImportError:
        parser.error("install the PC tool dependency first: python -m pip install pyserial")
    print("WASD/arrows move; Z fires; Space toggles held FIRE; Enter plays/restarts;")
    print("P pause; M auto demo; K density; Q quits. Input heartbeat is 30Hz.")
    serial_port = serial.Serial(args.port, 115200, timeout=0, write_timeout=0.2)
    sequence, held_fire, expires = 0, False, {}
    started = time.monotonic()
    mapping = {"a": LEFT, "d": RIGHT, "w": UP, "s": DOWN, "z": FIRE,
               "\r": START, "\n": START, "p": PAUSE, "m": DEMO, "k": LEVEL}

    def send(keys):
        nonlocal sequence
        serial_port.write(packet(keys, sequence))
        sequence = (sequence + 1) & 255
        incoming = serial_port.read(serial_port.in_waiting)
        if incoming:
            sys.stdout.write(incoming.decode("ascii", "replace"))
            sys.stdout.flush()

    try:
        if args.keys is not None:
            while not args.duration or time.monotonic() - started < args.duration:
                send(args.keys)
                time.sleep(1 / 30)
        else:
            with Console() as console:
                while not args.duration or time.monotonic() - started < args.duration:
                    now = time.monotonic()
                    for char in console.read():
                        if char in ("q", "\x03"):
                            return
                        if char == " ":
                            held_fire = not held_fire
                        if char in mapping:
                            expires[mapping[char]] = now + 0.18
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
            serial_port.write(packet(0, sequence))
        finally:
            serial_port.close()


if __name__ == "__main__":
    main()
