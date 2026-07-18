const FS = 8000;
const median = (values) => {
  if (!values.length) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
};

export function analyzePcg(samples, integrity) {
  if (samples.length < FS * 2) return { bpm: null, intervalMs: null, stability: null, quality: 0, reason: 'Cần ít nhất 2 giây tín hiệu' };
  let highPass = 0; let previous = samples[0]; let lowPass = 0; let envelope = 0;
  const env = new Float32Array(samples.length);
  let clipping = 0;
  for (let i = 0; i < samples.length; i += 1) {
    const x = samples[i] / 32768;
    if (Math.abs(samples[i]) >= 32600) clipping += 1;
    highPass = 0.985 * (highPass + x - previous);
    previous = x;
    lowPass += 0.095 * (highPass - lowPass); // low-pass ~120 Hz after removing drift
    envelope += 0.012 * ((lowPass * lowPass) - envelope);
    env[i] = envelope;
  }
  const envValues = Array.from(env);
  const base = median(envValues);
  const deviation = median(envValues.map((value) => Math.abs(value - base))) || 1e-9;
  const threshold = base + 2.5 * deviation;
  const peaks = [];
  const refractory = Math.round(FS * 0.06);
  for (let i = 1; i < env.length - 1; i += 1) {
    if (env[i] > threshold && env[i] >= env[i - 1] && env[i] > env[i + 1] && (!peaks.length || i - peaks.at(-1) > refractory)) peaks.push(i);
  }
  const s1 = [];
  for (let i = 0; i < peaks.length - 1; i += 1) {
    const gapMs = (peaks[i + 1] - peaks[i]) * 1000 / FS;
    if (gapMs >= 80 && gapMs <= 450) s1.push(peaks[i]);
  }
  const intervals = [];
  for (let i = 1; i < s1.length; i += 1) {
    const interval = (s1[i] - s1[i - 1]) * 1000 / FS;
    if (interval >= 333 && interval <= 1500) intervals.push(interval);
  }
  const intervalMs = median(intervals);
  const mad = intervalMs ? median(intervals.map((value) => Math.abs(value - intervalMs))) : 0;
  const bpm = intervals.length >= 3 ? Math.round(60000 / intervalMs) : null;
  const stability = intervalMs ? Math.round((mad / intervalMs) * 1000) / 10 : null;
  const packetPenalty = Math.min(35, (integrity.lossRate * 1000) + integrity.checksumFailures * 2);
  const clipPenalty = Math.min(35, clipping / samples.length * 4000);
  const coherence = Math.min(25, intervals.length * 6);
  const quality = Math.max(0, Math.min(100, Math.round(40 + coherence - packetPenalty - clipPenalty)));
  const reason = quality < 50 ? 'Kiểm tra tiếp xúc mic, nhiễu hoặc mất gói' : bpm === null ? 'Chưa đủ chu kỳ S1–S1 nhất quán' : 'Tín hiệu đủ để ước lượng';
  return { bpm, intervalMs: intervalMs ? Math.round(intervalMs) : null, stability, quality, reason, markers: s1.slice(-8), sampleOffset: samples.length };
}
