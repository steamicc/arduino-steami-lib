#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run a YAML-driven human qualification scenario on a real STeaMi board.

Qualification tests sit above unit/integration tiers: they verify that a
human-induced physical perturbation (breathing on a humidity sensor,
warming the board, covering a light sensor, moving near a distance
sensor...) causes a coherent change in the observed measurements.

The runner:

  1. loads tests/qualification/<driver>/qualify.yaml
  2. flashes the paired qualification sketch
  3. opens the serial port before reset
  4. resets the board through OpenOCD
  5. walks the YAML phases interactively
  6. evaluates declared assertions
  7. archives the full session under logs/qualification/

Usage:

    python scripts/qualify.py hts221
"""

from __future__ import annotations

import argparse
import datetime as dt
import re
import statistics
import subprocess
import sys
import time
import os
from pathlib import Path

import serial
import yaml
from serial.tools import list_ports

DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUDRATE = 115200
ROOT = Path(__file__).resolve().parent.parent


# ---------- low-level board plumbing ----------


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

    result = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)

    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, file=sys.stderr, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        raise SystemExit("Error: OpenOCD reset failed.")


def flash_qualification_sketch(driver: str) -> None:
    src_dir = ROOT / "tests" / "qualification" / driver

    if not src_dir.exists():
        raise SystemExit(f"Error: qualification directory not found for '{driver}'.")

    pio = ROOT / ".venv" / "bin" / "pio"
    
    if not pio.exists():
        raise SystemExit("Error: .venv/bin/pio not found. Run 'make setup' first.")
    
    cmd = [
        str(pio),
        "run",
        "-e",
        "steami",
        "-t",
        "upload",
    ]

    env = dict(os.environ)
    env["PLATFORMIO_SRC_DIR"] = str(src_dir)

    result = subprocess.run(
        cmd,
        env=env,
        cwd=ROOT,
    )

    if result.returncode != 0:
        raise SystemExit("Error: qualification sketch upload failed.")


# ---------- YAML loading ----------


def load_scenario(driver: str) -> dict:
    path = ROOT / "tests" / "qualification" / driver / "qualify.yaml"

    if not path.exists():
        raise SystemExit(f"Error: {path} not found.")

    with path.open("r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle)

    if not isinstance(data, dict):
        raise SystemExit("Error: qualification YAML root must be a mapping.")

    if "phases" not in data or not isinstance(data["phases"], list):
        raise SystemExit("Error: qualification YAML must define a phases list.")

    return data


# ---------- serial parsing ----------


PAIR_RE = re.compile(r"([a-zA-Z_]+)=(-?\d+(?:\.\d+)?)")


def parse_measurements(line: str) -> dict[str, float]:
    out: dict[str, float] = {}

    for key, value in PAIR_RE.findall(line):
        out[key] = float(value)

    return out


def capture_phase(
    ser: serial.Serial,
    seconds: float,
    wanted: list[str],
    log,
    phase_name: str,
) -> dict[str, list[float]]:
    captured = {key: [] for key in wanted}

    ser.reset_input_buffer()
    time.sleep(0.2)
    reset_board()
    deadline = time.monotonic() + seconds
    
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue

        text = raw.decode("utf-8", errors="replace").strip()
        stamp = timestamp()
        log.write(f"[{stamp}] SERIAL [{phase_name}] {text}\n")

        values = parse_measurements(text)

        for key in wanted:
            if key in values:
                captured[key].append(values[key])

    return captured


# ---------- metrics + assertions ----------


def average_metrics(samples: dict[str, list[float]]) -> dict[str, float]:
    metrics: dict[str, float] = {}

    for key, values in samples.items():
        if not values:
            continue
        metrics[key] = statistics.mean(values)

    return metrics


def evaluate_assertions(
    phase_name: str,
    phase_cfg: dict,
    metrics: dict[str, float],
    prior_metrics: dict[str, dict[str, float]],
    log,
) -> bool:
    assertions = phase_cfg.get("assert")
    if not assertions:
        return True

    passed = True

    for raw_key, expected in assertions.items():
        signal = raw_key.split("_")[0]

        if signal not in metrics:
            print(f"  FAIL: no captured values for '{signal}'")
            log.write(f"[{timestamp()}] ASSERT FAIL [{raw_key}] missing-signal\n")
            passed = False
            continue

        actual = metrics[signal]

        if raw_key.endswith("_delta_min"):
            ref_key = f"{signal}_delta_vs"
            ref_name = assertions.get(ref_key)

            if not isinstance(ref_name, str):
                print(f"  FAIL: missing '{ref_key}' for assertion '{raw_key}'")
                log.write(
                    f"[{timestamp()}] ASSERT FAIL [{phase_name}] {raw_key} missing-{ref_key}\n"
                )
                passed = False
                continue

            if ref_name not in prior_metrics:
                print(f"  FAIL: reference phase '{ref_name}' not found")
                log.write(
                    f"[{timestamp()}] ASSERT FAIL [{phase_name}] {raw_key} unknown-phase={ref_name}\n"
                )
                passed = False
                continue

            if signal not in prior_metrics[ref_name]:
                print(f"  FAIL: reference phase '{ref_name}' has no '{signal}' data")
                log.write(
                    f"[{timestamp()}] ASSERT FAIL [{phase_name}] {raw_key} missing-signal={signal}\n"
                )
                passed = False
                continue

            delta = actual - prior_metrics[ref_name][signal]
            ok = delta >= float(expected)
            verdict = "PASS" if ok else "FAIL"
            print(f"  {verdict}: {signal} delta >= {expected} (actual {delta:.2f})")
            log.write(
                f"[{timestamp()}] ASSERT {verdict} [{raw_key}] expected={expected} actual={delta:.2f}\n"
            )
            passed &= ok

        elif raw_key.endswith("_delta_max"):
            ref_name = assertions.get(f"{signal}_delta_vs")
            if ref_name not in prior_metrics or signal not in prior_metrics[ref_name]:
                print(f"  FAIL: reference phase '{ref_name}' missing")
                log.write(f"[{timestamp()}] ASSERT FAIL [{raw_key}] missing-reference\n")
                passed = False
                continue

            delta = actual - prior_metrics[ref_name][signal]
            ok = delta <= float(expected)
            verdict = "PASS" if ok else "FAIL"
            print(f"  {verdict}: {signal} delta <= {expected} (actual {delta:.2f})")
            log.write(
                f"[{timestamp()}] ASSERT {verdict} [{raw_key}] expected={expected} actual={delta:.2f}\n"
            )
            passed &= ok

        elif raw_key.endswith("_min"):
            ok = actual >= float(expected)
            verdict = "PASS" if ok else "FAIL"
            print(f"  {verdict}: {signal} avg >= {expected} (actual {actual:.2f})")
            log.write(
                f"[{timestamp()}] ASSERT {verdict} [{raw_key}] expected={expected} actual={actual:.2f}\n"
            )
            passed &= ok

        elif raw_key.endswith("_max"):
            ok = actual <= float(expected)
            verdict = "PASS" if ok else "FAIL"
            print(f"  {verdict}: {signal} avg <= {expected} (actual {actual:.2f})")
            log.write(
                f"[{timestamp()}] ASSERT {verdict} [{raw_key}] expected={expected} actual={actual:.2f}\n"
            )
            passed &= ok

    return passed


# ---------- misc ----------


def timestamp() -> str:
    return dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def open_log(driver: str):
    log_dir = ROOT / "logs" / "qualification"
    log_dir.mkdir(parents=True, exist_ok=True)

    name = dt.datetime.now().strftime(f"{driver}-%Y-%m-%dT%H-%M-%S.log")
    path = log_dir / name
    return path, path.open("w", encoding="utf-8")


# ---------- main runner ----------


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run a YAML-driven qualification scenario."
    )
    parser.add_argument("driver", help="Driver qualification scenario to run")
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"Serial port to use (default: {DEFAULT_PORT})",
    )
    args = parser.parse_args()

    scenario = load_scenario(args.driver)
    baudrate = int(scenario.get("baud", DEFAULT_BAUDRATE))

    port_name = steami_port(args.port)
    if port_name is None:
        print(
            "Error: no STeaMi serial device detected. "
            "Check that the board is connected and visible as /dev/ttyACM*.",
            file=sys.stderr,
        )
        return 1

    print(f"Flashing qualification sketch for {args.driver}...")
    flash_qualification_sketch(args.driver)

    log_path, log = open_log(args.driver)

    phase_metrics: dict[str, dict[str, float]] = {}
    overall_pass = True

    try:
        with serial.Serial(port_name, baudrate=baudrate, timeout=0.1) as ser:
            for phase in scenario["phases"]:
                name = phase["name"]
                prompt = phase["prompt"]
                duration = float(phase.get("capture_seconds", 5))
                wanted = phase.get("record", [])

                print()
                print(f"=== Phase: {name} ===")
                print(prompt)
                input()

                log.write(f"[{timestamp()}] PHASE [{name}]\n")
                log.write(f"[{timestamp()}] PROMPT [{prompt}]\n")

                samples = capture_phase(ser, duration, wanted, log, name)
                metrics = average_metrics(samples)
                phase_metrics[name] = metrics

                for key, value in metrics.items():
                    print(f"  {key}: avg={value:.2f}")
                    log.write(f"[{timestamp()}] METRIC [{name}] [{key}] avg={value:.2f}\n")

                phase_ok = evaluate_assertions(name, phase, metrics, phase_metrics, log)

                if phase.get("wait_for_confirm", False):
                    answer = input("Reading coherent? [y/N] ").strip().lower()
                    human_ok = answer == "y"
                    log.write(f"[{timestamp()}] HUMAN_CONFIRM [{name}] [{answer}]\n")
                    phase_ok &= human_ok

                overall_pass &= phase_ok

    finally:
        log.close()

    print()
    print(f"Qualification log archived to: {log_path}")

    return 0 if overall_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
