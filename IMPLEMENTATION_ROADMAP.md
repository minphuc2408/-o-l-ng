# Implementation Roadmap — Real PCG and ANC Calibration

## Fixed architecture

Two INMP441 microphones share I2S (`WS=GPIO21`, `SCK=GPIO22`, `SD=GPIO23`).
Mic1 is Left/Primary and Mic2 is Right/Reference. Firmware captures 48 kHz,
uses independent order-2 CIC instances to produce 8 kHz channels, runs NLMS ANC,
and calculates BPM locally on the ESP32. Normal USB UART output contains only
newline-delimited peak/BPM text; raw calibration data is binary only during an
explicit calibration session.

## Mandatory real-data calibration

1. Build and flash the firmware.
2. Put the final mechanical assembly in a stable common-noise environment; do
   not acquire patient data during calibration.
3. Run `tools/calibrate_anc.py` for 30 seconds with a host output folder outside
   the repository.
4. Review `anc_calibration_report.json`. A valid report contains frame integrity,
   correlation lag, input power, validation attenuation, and generated N/µ.
5. The tool updates `include/config.h`; rebuild and flash to activate the result.

The tool must not update firmware configuration when its quality gates fail.

## Measurement UI

The Vite/React console uses Web Serial in Chrome or Edge, parses the firmware's
BPM text events, and displays only the latest heart rate. The current detector
is explicitly scoped to resting measurements with accepted intervals of
600–1500 ms (approximately 40–100 BPM). A median-based rhythm lock rejects
early S2 candidates without relying on alternating odd/even peak state.

## Acceptance on real hardware

- Both I2S channels contain non-silent, non-clipped signals and no DMA overflow
  during a 60-second capture.
- A 30-second calibration has less than 1% sequence loss and produces a positive
  validation attenuation before it updates N and µ.
- The React console remains connected for ten minutes and updates only when a
  complete, valid BPM line is received.
- BPM is evaluated against an external reference in a documented research run;
  it is never represented as a diagnosis.
