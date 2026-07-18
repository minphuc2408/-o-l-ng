import { useSerialStream } from './hooks/useSerialStream';
import './styles.css';

export default function App() {
  const stream = useSerialStream();
  const bpm = stream.heartRate.bpm > 0 ? stream.heartRate.bpm : null;

  return (
    <main className="app">
      <header>
        <div>
          <p className="eyebrow">ESP32 HEART RATE</p>
          <h1>Nhịp tim</h1>
        </div>
        <button
          className={stream.isConnected ? 'disconnect' : ''}
          onClick={stream.isConnected ? stream.disconnect : stream.connect}
        >
          {stream.isConnected ? 'Ngắt kết nối' : 'Kết nối ESP32'}
        </button>
      </header>

      {stream.error && <p className="error">{stream.error}</p>}

      <section className="heart-rate" aria-live="polite">
        <span>Nhịp tim</span>
        <strong>
          {bpm ?? '—'}
          <small>BPM</small>
        </strong>
        <p>
          {!stream.isConnected
            ? 'Chưa kết nối'
            : bpm
              ? 'Cập nhật khi phát hiện peak mới'
              : 'Đang chờ đủ 8 khoảng peak hợp lệ'}
        </p>
      </section>
    </main>
  );
}
