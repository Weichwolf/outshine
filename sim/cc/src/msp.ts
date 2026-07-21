// MSP wire codec — the protocol iNav speaks. v1 ($M) for command < 256 and payload < 256, v2 ($X,
// DVB-S2 CRC) otherwise. The flightbox hub relays these frames verbatim between the CC and iNav, so the
// CC talks the exact same wire iNav uses on real hardware (CRSF is a later driver swap, same MSP payloads).

export function crc8DvbS2(c: number, b: number): number {
  c ^= b;
  for (let i = 0; i < 8; i++) c = (c & 0x80) ? ((c << 1) ^ 0xd5) & 0xff : (c << 1) & 0xff;
  return c;
}

/** Encode an MSP request frame (direction '<'). Picks v1 or v2 automatically. */
export function encode(cmd: number, payload: Uint8Array = new Uint8Array(0)): Uint8Array {
  if (cmd < 256 && payload.length < 256) {
    const f = new Uint8Array(6 + payload.length);
    f[0] = 0x24; f[1] = 0x4d; f[2] = 0x3c; f[3] = payload.length; f[4] = cmd; f.set(payload, 5);
    let c = 0; for (let i = 3; i < 5 + payload.length; i++) c ^= f[i];
    f[5 + payload.length] = c & 0xff;
    return f;
  }
  const f = new Uint8Array(9 + payload.length);
  f[0] = 0x24; f[1] = 0x58; f[2] = 0x3c; f[3] = 0;
  f[4] = cmd & 0xff; f[5] = cmd >> 8; f[6] = payload.length & 0xff; f[7] = payload.length >> 8;
  f.set(payload, 8);
  let c = 0; for (let i = 3; i < 8 + payload.length; i++) c = crc8DvbS2(c, f[i]);
  f[8 + payload.length] = c;
  return f;
}

export interface MspFrame { cmd: number; payload: Uint8Array; }

/** Streaming parser: feed received bytes, get back whole frames. Tolerates split/joined reads and skips
 *  stray bytes (the fanned hub stream carries only responses '>' / errors '!'). */
export class MspParser {
  private buf = new Uint8Array(0);
  push(data: Uint8Array): MspFrame[] {
    const merged = new Uint8Array(this.buf.length + data.length);
    merged.set(this.buf); merged.set(data, this.buf.length);
    const n = merged.length; const out: MspFrame[] = []; let i = 0;
    while (i + 3 <= n) {
      if (merged[i] !== 0x24 || (merged[i + 2] !== 0x3e && merged[i + 2] !== 0x21)) { i++; continue; } // '$' + '>'/'!'
      const typ = merged[i + 1];
      if (typ === 0x4d) {                                   // v1: $M> len cmd payload crc
        if (i + 5 > n) break;
        const ln = merged[i + 3], cmd = merged[i + 4];
        if (i + 6 + ln > n) break;
        out.push({ cmd, payload: merged.slice(i + 5, i + 5 + ln) }); i += 6 + ln;
      } else if (typ === 0x58) {                            // v2: $X> flag cmdL cmdH lenL lenH payload crc
        if (i + 8 > n) break;
        const cmd = merged[i + 4] | (merged[i + 5] << 8);
        const ln = merged[i + 6] | (merged[i + 7] << 8);
        if (i + 9 + ln > n) break;
        out.push({ cmd, payload: merged.slice(i + 8, i + 8 + ln) }); i += 9 + ln;
      } else i++;
    }
    this.buf = merged.slice(i);
    return out;
  }
}
