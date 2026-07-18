import { useCallback, useEffect, useRef, useState } from 'react';
import { SerialParser } from '../lib/serialParser';

const SAMPLE_RATE = 8000;
const HISTORY = SAMPLE_RATE * 4;

export function useSerialStream() {
  const portRef = useRef(null); const readerRef = useRef(null); const runningRef = useRef(false); const parserRef = useRef(new SerialParser()); const samplesRef = useRef([]);
  const [isConnected, setIsConnected] = useState(false); const [samples, setSamples] = useState([]); const [error, setError] = useState(null); const [integrity, setIntegrity] = useState({ validFrames: 0, checksumFailures: 0, sequenceGaps: 0, lossRate: 0 }); const [elapsedSeconds, setElapsedSeconds] = useState(0);
  const disconnect = useCallback(async () => {
    runningRef.current = false;
    try { await readerRef.current?.cancel(); } catch (_) {}
    try { readerRef.current?.releaseLock(); } catch (_) {}
    readerRef.current = null;
    try { await portRef.current?.close(); } catch (_) {}
    portRef.current = null; setIsConnected(false);
  }, []);
  const connect = useCallback(async () => {
    if (!navigator.serial) { setError('Trình duyệt này không hỗ trợ Web Serial. Hãy dùng Chrome hoặc Edge.'); return; }
    try {
      parserRef.current.clear(); samplesRef.current = []; setError(null);
      const port = await navigator.serial.requestPort(); await port.open({ baudRate: 921600 }); portRef.current = port; runningRef.current = true; setIsConnected(true);
      (async () => {
        try {
          const reader = port.readable.getReader(); readerRef.current = reader;
          while (runningRef.current) {
            const { value, done } = await reader.read(); if (done) break;
            if (value) for (const frame of (parserRef.current.pushBytes(value), parserRef.current.parse())) samplesRef.current.push(...frame);
            if (samplesRef.current.length > HISTORY) samplesRef.current.splice(0, samplesRef.current.length - HISTORY);
          }
        } catch (readError) { if (runningRef.current) setError(readError.message); } finally { try { readerRef.current?.releaseLock(); } catch (_) {} readerRef.current = null; setIsConnected(false); }
      })();
    } catch (connectError) { setError(connectError.message); setIsConnected(false); }
  }, []);
  useEffect(() => { if (!isConnected) return undefined; const started = performance.now(); const timer = setInterval(() => { setSamples([...samplesRef.current]); const stat = parserRef.current.stats; setIntegrity({ ...stat, lossRate: stat.sequenceGaps / Math.max(1, stat.validFrames + stat.sequenceGaps) }); setElapsedSeconds(Math.floor((performance.now() - started) / 1000)); }, 125); return () => clearInterval(timer); }, [isConnected]);
  useEffect(() => () => { disconnect(); }, [disconnect]);
  return { connect, disconnect, isConnected, samples, integrity, elapsedSeconds, error, supported: Boolean(navigator.serial) };
}
