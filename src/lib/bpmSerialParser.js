const PEAK_LINE =
  /^Peak detected \| interval=(\d+)ms \| amplitude=(\d+) \| BPM=(\d+)$/;
const MAX_LINE_LENGTH = 128;

/**
 * Parser streaming cho normal mode BPM-only.
 *
 * Firmware chỉ phát ASCII trong normal mode. Parser vẫn bỏ qua byte không in
 * được để không biến dữ liệu calibration nhị phân thành một kết quả BPM giả.
 */
export class BpmSerialParser {
  constructor() {
    this.clear();
  }

  pushBytes(bytes) {
    for (const byte of bytes) {
      if (byte === 13) continue;

      if (byte === 10) {
        this.consumeLine();
        this.line = '';
        continue;
      }

      if (byte >= 32 && byte <= 126) {
        if (this.line.length < MAX_LINE_LENGTH) {
          this.line += String.fromCharCode(byte);
        } else {
          this.line = '';
        }
      } else {
        this.line = '';
      }
    }
  }

  consumeLine() {
    const match = PEAK_LINE.exec(this.line);
    if (!match) return;

    const intervalMs = Number(match[1]);
    const amplitude = Number(match[2]);
    const bpm = Number(match[3]);
    if (
      !Number.isSafeInteger(intervalMs) ||
      !Number.isSafeInteger(amplitude) ||
      !Number.isSafeInteger(bpm)
    ) return;

    this.heartRate = {
      intervalMs,
      amplitude,
      bpm,
      peakCount: this.heartRate.peakCount + 1,
    };
  }

  clear() {
    this.line = '';
    this.heartRate = { intervalMs: null, amplitude: null, bpm: null, peakCount: 0 };
  }
}
