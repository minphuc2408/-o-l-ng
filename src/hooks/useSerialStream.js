import { useCallback, useEffect, useRef, useState } from 'react';
import { BpmSerialParser } from '../lib/bpmSerialParser';

export function useSerialStream() {
  const portRef = useRef(null); const readerRef = useRef(null); const runningRef = useRef(false); const parserRef = useRef(new BpmSerialParser());
  const [isConnected, setIsConnected] = useState(false); const [heartRate, setHeartRate] = useState({ intervalMs: null, amplitude: null, bpm: null, peakCount: 0 }); const [error, setError] = useState(null);
  const disconnect = useCallback(async () => {
    runningRef.current = false;
    try { await readerRef.current?.cancel(); } catch (_) {}
    try { readerRef.current?.releaseLock(); } catch (_) {}
    readerRef.current = null;
    try { await portRef.current?.close(); } catch (_) {}
    portRef.current = null; setIsConnected(false); setHeartRate({ intervalMs: null, amplitude: null, bpm: null, peakCount: 0 });
  }, []);
  const connect = useCallback(async () => {
    if (!navigator.serial) { setError('Trình duyệt này không hỗ trợ Web Serial. Hãy dùng Chrome hoặc Edge.'); return; }
    try {
      parserRef.current.clear(); setHeartRate({ intervalMs: null, amplitude: null, bpm: null, peakCount: 0 }); setError(null);
      const port = await navigator.serial.requestPort(); await port.open({ baudRate: 921600 }); portRef.current = port; runningRef.current = true; setIsConnected(true);
      (async () => {
        try {
          const reader = port.readable.getReader(); readerRef.current = reader;
          while (runningRef.current) {
            const { value, done } = await reader.read(); if (done) break;
            if (value) parserRef.current.pushBytes(value);
          }
        } catch (readError) { if (runningRef.current) setError(readError.message); } finally { try { readerRef.current?.releaseLock(); } catch (_) {} readerRef.current = null; setIsConnected(false); if (runningRef.current) setHeartRate({ intervalMs: null, amplitude: null, bpm: null, peakCount: 0 }); }
      })();
    } catch (connectError) { setError(connectError.message); setIsConnected(false); }
  }, []);
  useEffect(() => { if (!isConnected) return undefined; const timer = setInterval(() => { setHeartRate({ ...parserRef.current.heartRate }); }, 125); return () => clearInterval(timer); }, [isConnected]);
  useEffect(() => () => { disconnect(); }, [disconnect]);
  return { connect, disconnect, isConnected, heartRate, error, supported: Boolean(navigator.serial) };
}
