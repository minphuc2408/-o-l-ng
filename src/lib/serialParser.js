const FRAME_SIZE = 69;
const SYNC_0 = 0xaa;
const SYNC_1 = 0x55;

export class SerialParser {
  constructor() {
    this.buffer = [];
    this.stats = { validFrames: 0, checksumFailures: 0, sequenceGaps: 0, lastSequence: null };
  }

  pushBytes(bytes) {
    this.buffer.push(...bytes);
  }

  parse() {
    const frames = [];
    while (this.buffer.length >= FRAME_SIZE) {
      if (this.buffer[0] !== SYNC_0 || this.buffer[1] !== SYNC_1 || this.buffer[3] !== 32) {
        this.buffer.shift();
        continue;
      }
      let checksum = 0;
      for (let i = 0; i < FRAME_SIZE - 1; i += 1) checksum ^= this.buffer[i];
      if (checksum !== this.buffer[FRAME_SIZE - 1]) {
        this.stats.checksumFailures += 1;
        this.buffer.shift();
        continue;
      }
      const bytes = this.buffer.splice(0, FRAME_SIZE);
      const sequence = bytes[2];
      if (this.stats.lastSequence !== null) {
        const expected = (this.stats.lastSequence + 1) & 0xff;
        if (sequence !== expected) this.stats.sequenceGaps += (sequence - expected + 256) & 0xff;
      }
      this.stats.lastSequence = sequence;
      this.stats.validFrames += 1;
      const samples = new Int16Array(32);
      for (let i = 0; i < 32; i += 1) samples[i] = bytes[4 + i * 2] | (bytes[5 + i * 2] << 8);
      frames.push(samples);
    }
    return frames;
  }

  clear() {
    this.buffer = [];
    this.stats = { validFrames: 0, checksumFailures: 0, sequenceGaps: 0, lastSequence: null };
  }
}
