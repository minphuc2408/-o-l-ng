# Serial Protocol — PCG Measurement and ANC Calibration

UART runs at **921600 baud, 8-N-1**. The UART is binary-only while streaming;
firmware must not print logs to it.

## Measurement stream

Normal operation emits one 69-byte frame per 32 clean PCG samples:

`AA 55 | sequence:u8 | count:u8=32 | clean_pcm:int16[32] LE | xor:u8`

The XOR is calculated across every byte before the final checksum. The browser
must seek `AA 55`, require `count=32`, validate XOR, and use sequence gaps as a
data-quality metric.

## Calibration control and stream

The host sends this 8-byte command to request a real calibration session:

`A5 5A | 01 | duration_ms:u32 LE | xor:u8`

The duration must be 10–60 seconds. The checksum covers all preceding command
bytes. Firmware then temporarily replaces the measurement stream with 261-byte
calibration frames:

`AA 56 | sequence:u8 | count:u8=32 | primary_cic:int32[32] LE | reference_cic:int32[32] LE | xor:u8`

Each pair is the exact 8 kHz CIC output that normally enters ANC, before ANC is
applied. Firmware automatically returns to the 69-byte measurement stream when
the requested duration expires. The calibration host must discard malformed or
old measurement frames while it seeks valid `AA 56` 261-byte frames.

## Real calibration procedure

Build and flash firmware, place the final two-microphone assembly in a stable
noise field, and run:

```powershell
pip install -r tools/requirements.txt
python tools/calibrate_anc.py --port COM5 --duration 30 --out D:\pcg-captures\cal-001
```

The tool rejects silent/clipped channels, weak correlation, non-causal acoustic
placement, excessive frame loss, divergence, or non-positive validation
attenuation. On success it writes the real session/report to `--out` and updates
only `ANC_NUM_TAPS` and `ANC_MU_INIT` in `include/config.h`. Rebuild and flash
the firmware afterward.
