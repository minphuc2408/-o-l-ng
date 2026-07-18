import { useEffect, useMemo, useRef } from 'react';
import { useSerialStream } from './hooks/useSerialStream';
import { analyzePcg } from './lib/pcgAnalysis';
import './styles.css';

function Waveform({ samples, markers }) {
  const ref = useRef(null);
  useEffect(() => {
    const canvas = ref.current; if (!canvas) return;
    const rect = canvas.getBoundingClientRect(); const dpr = window.devicePixelRatio || 1;
    canvas.width = rect.width * dpr; canvas.height = rect.height * dpr; const ctx = canvas.getContext('2d'); ctx.scale(dpr, dpr);
    const { width, height } = rect; ctx.fillStyle = '#08111d'; ctx.fillRect(0, 0, width, height);
    ctx.strokeStyle = '#1e3349'; ctx.lineWidth = 1; for (let i = 1; i < 5; i += 1) { const y = i * height / 5; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke(); }
    if (samples.length) {
      const step = Math.max(1, Math.floor(samples.length / Math.max(1, width))); let max = 1000; for (const value of samples) max = Math.max(max, Math.abs(value));
      ctx.strokeStyle = '#50d6b0'; ctx.lineWidth = 1.2; ctx.beginPath(); for (let x = 0; x < width; x += 1) { const start = Math.floor(x * samples.length / width); let value = 0; for (let i = start; i < Math.min(samples.length, start + step); i += 1) value += samples[i]; value /= step; const y = height / 2 - value / max * height * .44; x ? ctx.lineTo(x, y) : ctx.moveTo(x, y); } ctx.stroke();
      ctx.strokeStyle = '#f4c95d'; for (const marker of markers) { const x = marker / samples.length * width; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke(); }
    }
  }, [samples, markers]);
  return <canvas className="canvas" ref={ref} aria-label="Dạng sóng phonocardiogram" />;
}

function Spectrum({ samples }) {
  const ref = useRef(null);
  useEffect(() => {
    const canvas = ref.current; if (!canvas) return; const rect = canvas.getBoundingClientRect(); const dpr = window.devicePixelRatio || 1; canvas.width = rect.width * dpr; canvas.height = rect.height * dpr; const ctx = canvas.getContext('2d'); ctx.scale(dpr, dpr); ctx.fillStyle = '#08111d'; ctx.fillRect(0, 0, rect.width, rect.height);
    const input = samples.slice(-512); if (input.length < 512) return; const bins = 32; const values = [];
    for (let k = 1; k <= bins; k += 1) { let re = 0; let im = 0; for (let n = 0; n < input.length; n += 1) { const angle = 2 * Math.PI * k * n / input.length; re += input[n] * Math.cos(angle); im -= input[n] * Math.sin(angle); } values.push(Math.sqrt(re * re + im * im)); }
    const max = Math.max(...values, 1); const bar = rect.width / bins; values.forEach((value, i) => { const h = value / max * (rect.height - 18); ctx.fillStyle = '#4778d9'; ctx.fillRect(i * bar + 2, rect.height - h, bar - 4, h); }); ctx.fillStyle = '#9fb3c8'; ctx.font = '11px sans-serif'; ctx.fillText('0–500 Hz', 8, 14);
  }, [samples]);
  return <canvas className="canvas spectrum" ref={ref} aria-label="Phổ PCG 0 đến 500 Hertz" />;
}

const Metric = ({ label, value, unit, note }) => <article className="metric"><span>{label}</span><strong>{value ?? '—'}<small>{value !== null && value !== undefined ? unit : ''}</small></strong><em>{note}</em></article>;

export default function App() {
  const stream = useSerialStream();
  const analysis = useMemo(() => analyzePcg(stream.samples, stream.integrity), [stream.samples, stream.integrity]);
  const markers = analysis.markers?.map((point) => point - (analysis.sampleOffset - stream.samples.length)).filter((point) => point >= 0) ?? [];
  const duration = `${Math.floor(stream.elapsedSeconds / 60).toString().padStart(2, '0')}:${(stream.elapsedSeconds % 60).toString().padStart(2, '0')}`;
  return <main className="app"><header><div><p className="eyebrow">PCG RESEARCH CONSOLE</p><h1>Âm tim thời gian thực</h1><p className="subtle">Ước lượng từ phonocardiogram; không dùng để chẩn đoán.</p></div><button className={stream.isConnected ? 'disconnect' : ''} onClick={stream.isConnected ? stream.disconnect : stream.connect}>{stream.isConnected ? 'Ngắt kết nối' : 'Kết nối ESP32'}</button></header>
    {stream.error && <p className="error">{stream.error}</p>}
    <section className="status"><span className={stream.isConnected ? 'dot online' : 'dot'} />{stream.isConnected ? 'Đang nhận PCG thật / ANC active' : 'Chưa kết nối'}<span>8 kHz</span><span>Calibration: tool USB riêng</span><span>Phiên {duration}</span><span>{stream.integrity.validFrames} frames</span></section>
    <section className="metrics"><Metric label="Nhịp tim ước lượng" value={analysis.bpm} unit="BPM" note="Từ chu kỳ S1–S1" /><Metric label="Chu kỳ S1–S1" value={analysis.intervalMs} unit="ms" note="Median cửa sổ hiện tại" /><Metric label="Độ ổn định" value={analysis.stability} unit="%" note="MAD / chu kỳ" /><Metric label="Chất lượng tín hiệu" value={analysis.quality} unit="/100" note={analysis.reason} /></section>
    <section className="grid"><article className="panel wide"><div className="panel-title"><div><h2>Dạng sóng PCG</h2><p>Xanh: tín hiệu sau ANC · Vàng: S1 được ước lượng</p></div><span>{analysis.quality >= 60 ? 'ĐỦ CHẤT LƯỢNG' : 'KIỂM TRA TÍN HIỆU'}</span></div><Waveform samples={stream.samples} markers={markers} /></article><article className="panel"><div className="panel-title"><div><h2>Phổ âm tim</h2><p>Dải quan sát 0–500 Hz</p></div></div><Spectrum samples={stream.samples} /></article></section>
    <section className="grid details"><article className="panel"><h2>Tính toàn vẹn dữ liệu</h2><dl><dt>Mất frame</dt><dd>{(stream.integrity.lossRate * 100).toFixed(2)}%</dd><dt>Checksum lỗi</dt><dd>{stream.integrity.checksumFailures}</dd><dt>Sequence gap</dt><dd>{stream.integrity.sequenceGaps}</dd></dl></article><article className="panel"><h2>Ghi chú nghiên cứu</h2><p>Khoảng tham chiếu nhịp nghỉ 60–100 BPM chỉ có ý nghĩa khi đang nghỉ, bình tĩnh và khỏe. Kết quả hiển thị là tín hiệu nghiên cứu từ âm tim, không thay thế ECG hay đánh giá lâm sàng.</p></article></section>
  </main>;
}
