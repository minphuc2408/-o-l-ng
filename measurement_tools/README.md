# Measurement tools

The normal firmware stream is **BPM-only text**. Capture a session with:

```powershell
.\env\Scripts\python.exe .\measurement_tools\record_bpm_measurement.py --port COM5 --duration 30 --name trial_001
```

This creates a CSV containing `event_index`, host time, peak interval, envelope
amplitude and BPM, plus a capture report inside `measurement_data/`.

`evaluate_pcg_csv.py` remains available only for previously captured PCM CSV
files. The evaluator rejects input and output paths outside `measurement_data/`:

```powershell
.\env\Scripts\python.exe .\measurement_tools\evaluate_pcg_csv.py .\measurement_data\trial_001.csv
```

Use the Web Serial interface or a standard Serial Monitor at 921600 baud for
the current BPM-only stream. Do not open either while running the separate
binary ANC calibration tool.
