# Serial Protocol — BPM-only Measurement and ANC Calibration

UART runs at **921600 baud, 8-N-1**.

## Normal measurement mode

Normal operation is text-only. Firmware does not transmit PCM frames. After
each detected heart-sound peak it emits one newline-terminated ASCII line:

```text
Peak detected | interval=XXXms | amplitude=YYY | BPM=XX
```

`interval=0` identifies the first accepted peak. `amplitude` is the rounded
envelope value at the upward threshold crossing.

The resting-mode rhythm lock starts with an expected RR of 900 ms, then uses
the median of accepted RR intervals. A candidate earlier than 75% of the
expected RR is classified as S2 and does not update the last accepted peak:

```text
Candidate rejected | interval=XXXms | amplitude=YYY | reason=S2
```

Valid RR intervals are 600–1500 ms (approximately 40–100 BPM). `BPM=0` is
emitted until 8 consecutive valid intervals are available. An interval above
1500 ms resets the interval history so stale BPM is not reported as current.

A Serial Monitor configured for 921600 baud can display normal mode directly
without binary characters.

The browser consumes complete lines and accepts only the exact format above.
Startup diagnostics such as `[SYSTEM ERROR] ...` are text but are not interpreted
as heart-rate measurements.

## Calibration control and stream

Calibration remains a separate, explicitly requested binary mode. The host
sends this 8-byte command:

`A5 5A | 01 | duration_ms:u32 LE | xor:u8`

The duration must be 10–60 seconds. The checksum covers all preceding command
bytes. Firmware temporarily replaces BPM text output with 261-byte calibration
frames:

`AA 56 | sequence:u8 | count:u8=32 | primary_cic:int32[32] LE | reference_cic:int32[32] LE | xor:u8`

Each pair is the exact 8 kHz CIC output that normally enters ANC, before ANC.
When the requested duration expires, firmware resets the heart-rate detector
and automatically returns to text-only BPM mode.

Binary characters are expected if a generic Serial Monitor is left open during
calibration. Close the monitor and let the calibration tool own the serial port.

## Real calibration procedure

Build and flash firmware, place the final two-microphone assembly in a stable
noise field, and run:

```powershell
pip install -r tools/requirements.txt
python tools/calibrate_anc.py --port COM5 --duration 30 --out D:\pcg-captures\cal-001
```

The tool rejects silent/clipped channels, weak correlation, non-causal acoustic
placement, excessive frame loss, divergence, or non-positive validation
attenuation. On success it writes the session/report to `--out` and updates
only `ANC_NUM_TAPS` and `ANC_MU_INIT` in `include/config.h`.
