#!/usr/bin/env python3
"""Evaluate a CSV exported by the PCG Research Console.

This is a research quality-control tool, not a diagnostic tool. It reports
signal and transport-derived metrics and writes a JSON report beside the CSV
unless --output is supplied.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import numpy as np

SAMPLE_RATE = 8000
CLIP_LIMIT = 32600
PROJECT_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = PROJECT_ROOT / 'measurement_data'


def project_path(path: Path, label: str) -> Path:
    """Resolve a path and reject files outside this project."""
    resolved = path.resolve()
    try:
        resolved.relative_to(PROJECT_ROOT)
    except ValueError as error:
        raise ValueError(f'{label} must be inside {PROJECT_ROOT}') from error
    return resolved


def measurement_path(path: Path, label: str) -> Path:
    resolved = project_path(path, label)
    try:
        resolved.relative_to(DATA_DIR)
    except ValueError as error:
        raise ValueError(f'{label} must be inside {DATA_DIR}') from error
    return resolved


def median(values: np.ndarray) -> float:
    return float(np.median(values)) if values.size else 0.0


def load_csv(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    with path.open('r', encoding='utf-8-sig', newline='') as handle:
        reader = csv.DictReader(handle)
        required = {'sample_index', 'relative_time_ms', 'frame_sequence', 'clean_pcm'}
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise ValueError(f'CSV must contain: {", ".join(sorted(required))}')
        rows = list(reader)
    if not rows:
        raise ValueError('CSV contains no samples.')
    return (
        np.asarray([int(row['sample_index']) for row in rows], dtype=np.int64),
        np.asarray([float(row['relative_time_ms']) for row in rows], dtype=np.float64),
        np.asarray([int(row['frame_sequence']) for row in rows], dtype=np.uint8),
        np.asarray([int(row['clean_pcm']) for row in rows], dtype=np.int16),
    )


def estimate_pcg(samples: np.ndarray) -> dict:
    if samples.size < SAMPLE_RATE * 2:
        return {'bpm_estimate': None, 's1_s1_median_ms': None, 'stability_percent': None,
                'quality_score': 0, 'reason': 'Need at least 2 seconds of signal.'}
    x = samples.astype(np.float64) / 32768.0
    high_pass = np.empty_like(x)
    previous = x[0]
    hp = low = envelope = 0.0
    for i, value in enumerate(x):
        hp = 0.985 * (hp + value - previous)
        previous = value
        low += 0.095 * (hp - low)
        envelope += 0.012 * ((low * low) - envelope)
        high_pass[i] = envelope
    base = median(high_pass)
    deviation = median(np.abs(high_pass - base)) or 1e-9
    threshold = base + 2.5 * deviation
    peaks: list[int] = []
    refractory = round(SAMPLE_RATE * 0.06)
    for i in range(1, high_pass.size - 1):
        if high_pass[i] > threshold and high_pass[i] >= high_pass[i - 1] and high_pass[i] > high_pass[i + 1] and (not peaks or i - peaks[-1] > refractory):
            peaks.append(i)
    s1 = [peaks[i] for i in range(len(peaks) - 1) if 80 <= (peaks[i + 1] - peaks[i]) * 1000 / SAMPLE_RATE <= 450]
    intervals = np.asarray([(s1[i] - s1[i - 1]) * 1000 / SAMPLE_RATE for i in range(1, len(s1)) if 333 <= (s1[i] - s1[i - 1]) * 1000 / SAMPLE_RATE <= 1500])
    interval = median(intervals)
    mad = median(np.abs(intervals - interval)) if interval else 0.0
    clip_pct = float(np.mean(np.abs(samples.astype(np.int32)) >= CLIP_LIMIT) * 100)
    quality = max(0, min(100, round(40 + min(25, intervals.size * 6) - min(35, clip_pct * 40))))
    return {'bpm_estimate': round(60000 / interval) if intervals.size >= 3 else None,
            's1_s1_median_ms': round(interval) if interval else None,
            'stability_percent': round(mad / interval * 100, 1) if interval else None,
            'quality_score': quality,
            'reason': 'Signal is sufficient for an estimate.' if quality >= 50 and intervals.size >= 3 else 'Insufficient consistent S1-S1 cycles or signal quality.'}


def main() -> None:
    parser = argparse.ArgumentParser(description='Evaluate a PCG measurement CSV.')
    parser.add_argument('csv_file', type=Path)
    parser.add_argument('--output', type=Path, help='JSON report path (default: beside CSV)')
    args = parser.parse_args()
    csv_file = measurement_path(args.csv_file, 'CSV input')
    indexes, timestamps_ms, sequences, samples = load_csv(csv_file)
    expected_indexes = np.arange(indexes.size)
    frame_sequences = sequences[::32]
    sequence_steps = (np.diff(frame_sequences.astype(np.int16)) + 256) % 256
    missing_frames = int(np.maximum(sequence_steps - 1, 0).sum())
    frame_loss_percent = round(missing_frames / max(1, frame_sequences.size + missing_frames) * 100, 4)
    report = {
        'source_csv': str(csv_file.relative_to(PROJECT_ROOT)),
        'sample_rate_hz': SAMPLE_RATE,
        'samples': int(samples.size),
        'duration_s': round(float(samples.size / SAMPLE_RATE), 3),
        'valid_frames': int(frame_sequences.size),
        'sequence_missing_frames': missing_frames,
        'sequence_loss_percent': frame_loss_percent,
        'index_gaps': int(np.count_nonzero(indexes != expected_indexes)),
        'timestamp_error_ms_max': round(float(np.max(np.abs(timestamps_ms - indexes * 1000 / SAMPLE_RATE))), 6),
        'dc_offset_pcm': round(float(np.mean(samples)), 2),
        'rms_pcm': round(float(np.sqrt(np.mean(samples.astype(np.float64) ** 2))), 2),
        'peak_pcm': int(np.max(np.abs(samples.astype(np.int32)))),
        'clipping_percent': round(float(np.mean(np.abs(samples.astype(np.int32)) >= CLIP_LIMIT) * 100), 4),
        'pcg_estimate': estimate_pcg(samples),
        'disclaimer': 'Research quality-control output only; it is not a clinical diagnosis.',
    }
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    output = measurement_path(args.output, 'Report output') if args.output else DATA_DIR / f'{csv_file.stem}_report.json'
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))
    print(f'Wrote report: {output}')


if __name__ == '__main__':
    main()
