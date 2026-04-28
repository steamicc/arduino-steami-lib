#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reset the board via OpenOCD and capture serial output for a fixed duration.

Designed to land boot-time logs that miniterm misses on STeaMi: the
serial port is opened *before* the reset, the input buffer is flushed,
and only then is OpenOCD asked to reset. Reading happens against a
post-reset deadline so DURATION reflects actual capture time rather
than wall-clock time including the reset itself.
"""
import argparse
import subprocess
import sys
import time

import serial
from serial.tools import list_ports


DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUDRATE = 115200
DEFAULT_DURATION = 10


def steami_port(default_port: str) -> str | None:
    ports = list(list_ports.comports())

    if not ports:
        return None

    for port in ports:
        if port.device == default_port:
            return port.device

    for port in ports:
        if port.device.startswith("/dev/ttyACM"):
            return port.device

    return None


def reset_board() -> None:
    cmd = [
        "openocd",
        "-f",
        "interface/cmsis-dap.cfg",
        "-f",
        "target/stm32wbx.cfg",
        "-c",
        "init; reset; shutdown",
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, file=sys.stderr, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        raise SystemExit("Error: OpenOCD reset failed.")

def capture_serial(port_name: str, baudrate: int, duration: float) -> int:
    try:
        with serial.Serial(port_name, baudrate=baudrate, timeout=0.1) as ser:
            time.sleep(0.2)
            ser.reset_input_buffer()

            # Reset first, then start the deadline. Otherwise the ~1-2 s
            # OpenOCD takes eats into the requested duration and short
            # captures (DURATION=2) end up empty.
            reset_board()
            deadline = time.monotonic() + duration

            while time.monotonic() < deadline:
                line = ser.readline()
                if not line:
                    continue

                text = line.decode("utf-8", errors="replace")
                print(text, end="", flush=True)

    except serial.SerialException as exc:
        print(f"Error: failed to open serial port {port_name}: {exc}", file=sys.stderr)
        return 1

    return 0

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Reset the board via OpenOCD and capture serial output."
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=DEFAULT_DURATION,
        help=f"Capture duration in seconds (default: {DEFAULT_DURATION})",
    )
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"Serial port to use (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=DEFAULT_BAUDRATE,
        help=f"Serial baudrate (default: {DEFAULT_BAUDRATE})",
    )
    args = parser.parse_args()

    port_name = steami_port(args.port)
    if port_name is None:
        print(
            "Error: no STeaMi serial device detected. "
            "Check that the board is connected and visible as /dev/ttyACM*.",
            file=sys.stderr,
        )
        return 1

    return capture_serial(port_name, args.baudrate, args.duration)


if __name__ == "__main__":
    raise SystemExit(main())
