#!/usr/bin/env python3
"""Capture PCG UART frames directly into this project's measurement_data folder.

The recorder accepts only checksum-valid normal-measurement frames (AA 55),
preserves the frame sequence and derives timing from the 8 kHz sample clock.
It never writes a capture outside the project directory.
"""
from __future__ import annotations

import argparse
import csv
import json
import time
from datetime import datetime
from pathlib import Path

import serial

SAMPLE_RATE = 8000
FRAME_SIZE = 69
SYNC = bytes((0xAA, 0x55))
PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = PROJECT_ROOT / 'measurement_data'


class FrameParser:
    def __init__(self) -> None:
        self.buffer = bytearray()
        self.valid_frames = 0
        self.checksum_failures = 0
        self.sequence_gaps = 0
        self.last_sequence: int | None = None

    def push(self, data: bytes) -> list[tuple[int, list[int]]]:
        self.buffer.extend(data)
        frames: list[tuple[int, list[int]]] = []
        while len(self.buffer) >= FRAME_SIZE:
            if self.buffer[:2] != SYNC or self.buffer[3] != 32:
                del self.buffer[0]
                continue
            frame = self.buffer[:FRAME_SIZE]
            checksum = 0
            for value in frame[:-1]:
                checksum ^= value
            if checksum != frame[-1]:
                self.checksum_failures += 1
                del self.buffer[0]
                continue
            del self.buffer[:FRAME_SIZE]
            sequence = frame[2]
            if self.last_sequence is not None:
                self.sequence_gaps += (sequence - ((self.last_sequence + 1) & 0xFF)) & 0xFF
            self.last_sequence = sequence
            samples = [int.from_bytes(frame[4 + index * 2:6 + index * 2], 'little', signed=True) for index in range(32)]
            self.valid_frames += 1
            frames.append((sequence, samples))
        return frames


def capture(port_name: str, duration_s: float, output: Path) -> dict:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    parser = FrameParser()
    sample_index = 0
    started = time.monotonic()
    with serial.Serial(port_name, 921600, timeout=0.25) as port, output.open('w', newline='', encoding='utf-8-sig') as handle:
        writer = csv.writer(handle)
        writer.writerow(['sample_index', 'relative_time_ms', 'frame_sequence', 'clean_pcm'])
        while time.monotonic() - started < duration_s:
            received = port.read(port.in_waiting or 1)
            for sequence, samples in parser.push(received):
                writer.writerows((sample_index + i, f'{(sample_index + i) * 1000 / SAMPLE_RATE:.3f}', sequence, sample) for i, sample in enumerate(samples))
                sample_index += len(samples)
            handle.flush()
    return {
        'capture_csv': str(output.relative_to(PROJECT_ROOT)),
        'sample_rate_hz': SAMPLE_RATE,
        'samples': sample_index,
        'duration_s': round(sample_index / SAMPLE_RATE, 3),
        'valid_frames': parser.valid_frames,
        'checksum_failures': parser.checksum_failures,
        'sequence_gaps': parser.sequence_gaps,
        'sequence_loss_percent': round(parser.sequence_gaps / max(1, parser.valid_frames + parser.sequence_gaps) * 100, 4),
    }


def main() -> None:
    argument_parser = argparse.ArgumentParser(description='Capture PCG data into project-root measurement_data/.')
    argument_parser.add_argument('--port', required=True, help='ESP32 serial port, e.g. COM5')
    argument_parser.add_argument('--duration', type=float, default=30, help='Capture duration in seconds (default: 30)')
    argument_parser.add_argument('--name', help='File stem only; no path is accepted')
    args = argument_parser.parse_args()
    if args.duration <= 0:
        argument_parser.error('--duration must be positive.')
    if args.name and (Path(args.name).name != args.name or Path(args.name).suffix):
        argument_parser.error('--name must be a filename stem, not a path or extension.')
    stem = args.name or f'pcg_measurement_{datetime.now():%Y%m%d_%H%M%S}'
    output = DATA_DIR / f'{stem}.csv'
    report = capture(args.port, args.duration, output)
    report_path = DATA_DIR / f'{stem}_capture_report.json'
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    print(f'Capture: {output.relative_to(PROJECT_ROOT)}')
    print(f'Report: {report_path.relative_to(PROJECT_ROOT)}')


if __name__ == '__main__':
    main()
