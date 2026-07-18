#!/usr/bin/env python3
"""Capture newline-delimited BPM events from the ESP32 normal UART mode."""
from __future__ import annotations

import argparse
import csv
import json
import re
import time
from datetime import datetime
from pathlib import Path

import serial

UART_BAUD_RATE = 921600
PEAK_LINE = re.compile(
    rb"^Peak detected \| interval=(\d+)ms \| amplitude=(\d+) \| BPM=(\d+)$"
)
REJECTED_LINE = re.compile(
    rb"^Candidate rejected \| interval=\d+ms \| amplitude=\d+ \| reason=S2$"
)
MAX_LINE_LENGTH = 128
PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = PROJECT_ROOT / "measurement_data"


class BpmLineParser:
    """Incremental ASCII line parser matching the browser's BPM protocol."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.invalid_lines = 0
        self.rejected_candidates = 0

    def push(self, data: bytes) -> list[tuple[int, int, int]]:
        self.buffer.extend(data)
        events: list[tuple[int, int, int]] = []

        while b"\n" in self.buffer:
            raw_line, _, remainder = self.buffer.partition(b"\n")
            self.buffer = bytearray(remainder)
            line = raw_line.rstrip(b"\r")
            match = PEAK_LINE.fullmatch(line)
            if match:
                events.append(
                    (int(match.group(1)), int(match.group(2)), int(match.group(3)))
                )
            elif REJECTED_LINE.fullmatch(line):
                self.rejected_candidates += 1
            elif line:
                self.invalid_lines += 1

        if len(self.buffer) > MAX_LINE_LENGTH:
            self.buffer.clear()
            self.invalid_lines += 1

        return events


def capture(port_name: str, duration_s: float, output: Path) -> dict:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    parser = BpmLineParser()
    event_count = 0
    valid_bpm_count = 0
    started = time.monotonic()

    with (
        serial.Serial(port_name, UART_BAUD_RATE, timeout=0.25) as port,
        output.open("w", newline="", encoding="utf-8-sig") as handle,
    ):
        writer = csv.writer(handle)
        writer.writerow(
            ["event_index", "host_elapsed_ms", "interval_ms", "amplitude", "bpm"]
        )

        while time.monotonic() - started < duration_s:
            received = port.read(port.in_waiting or 1)
            for interval_ms, amplitude, bpm in parser.push(received):
                elapsed_ms = round((time.monotonic() - started) * 1000, 3)
                writer.writerow([event_count, elapsed_ms, interval_ms, amplitude, bpm])
                event_count += 1
                if bpm > 0:
                    valid_bpm_count += 1
            handle.flush()

    return {
        "capture_csv": str(output.relative_to(PROJECT_ROOT)),
        "uart_baud_rate": UART_BAUD_RATE,
        "capture_duration_s": duration_s,
        "peak_events": event_count,
        "events_with_bpm": valid_bpm_count,
        "rejected_s2_candidates": parser.rejected_candidates,
        "ignored_non_bpm_lines": parser.invalid_lines,
    }


def main() -> None:
    argument_parser = argparse.ArgumentParser(
        description="Capture ESP32 BPM-only UART events into measurement_data/."
    )
    argument_parser.add_argument("--port", required=True, help="ESP32 serial port, e.g. COM5")
    argument_parser.add_argument(
        "--duration", type=float, default=30, help="Capture duration in seconds (default: 30)"
    )
    argument_parser.add_argument("--name", help="File stem only; no path is accepted")
    args = argument_parser.parse_args()

    if args.duration <= 0:
        argument_parser.error("--duration must be positive.")
    if args.name and (Path(args.name).name != args.name or Path(args.name).suffix):
        argument_parser.error("--name must be a filename stem, not a path or extension.")

    stem = args.name or f"bpm_measurement_{datetime.now():%Y%m%d_%H%M%S}"
    output = DATA_DIR / f"{stem}.csv"
    report = capture(args.port, args.duration, output)
    report_path = DATA_DIR / f"{stem}_capture_report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"Capture: {output.relative_to(PROJECT_ROOT)}")
    print(f"Report: {report_path.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
