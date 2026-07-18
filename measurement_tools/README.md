# Measurement tools

All generated measurement files are restricted to `../measurement_data/`.
No script in this folder accepts an output path outside the project root.

Capture a 30-second PCG session from ESP32:

```powershell
.\env\Scripts\python.exe .\measurement_tools\record_pcg_measurement.py --port COM5 --duration 30 --name trial_001
```

This creates only these files inside the repository:

```
measurement_data/trial_001.csv
measurement_data/trial_001_capture_report.json
```

Evaluate a saved measurement. The evaluator rejects input and output paths
outside `measurement_data/`:

```powershell
.\env\Scripts\python.exe .\tools\evaluate_pcg_csv.py .\measurement_data\trial_001.csv
```
