// Headless Command Center — a client of the running flightbox hub, exactly like the WASM CC.
//
// The flightbox (flightbox/server.c) owns the single MSP link to iNav and multiplexes it: it polls
// telemetry and fans the raw iNav downlink out to every connected CC, and forwards each CC's command
// frames up to iNav. So arbitrarily many command centers — this one, the browser, several test runners —
// receive the same telemetry and send commands in parallel, with no fixed binding: connect or leave any
// time while flightbox runs. There is only ever one flightbox + one aircraft; the CCs come and go.
//
// This client READS the fanned stream into `t` (it does not poll — the hub already does, at TELEM_HZ) and
// SENDS commands fire-and-forget (no request/response: the downlink is a shared broadcast, so a reply
// can't be owned by one requester). iNav flies natively; the CC only commands.

import { WebSocket } from 'ws';
import { encode, MspParser } from './msp.js';

export interface Telem {
  armed?: boolean; calibrating?: boolean;
  navState?: number; navWp?: number;          // MSP_NAV_STATUS (iNav MW_NAV_STATE_*): 3/4 hold, 5 WP, 8/9 land
  aglM?: number; vs?: number;                  // MSP_ALTITUDE: home-relative altitude + vario
  lat?: number; lon?: number; altAsl?: number; gs?: number; course?: number; fix?: number; sats?: number;
  roll?: number; pitch?: number; yaw?: number; // FULL_LOCAL_POSE (0.1 deg)
  updated?: number;                            // Date.now() of the last frame
}

const rd = { i16: (p: Uint8Array, o: number) => (p[o] | (p[o + 1] << 8)) << 16 >> 16,
             u16: (p: Uint8Array, o: number) => p[o] | (p[o + 1] << 8),
             i32: (p: Uint8Array, o: number) => (p[o] | (p[o + 1] << 8) | (p[o + 2] << 16) | (p[o + 3] << 24)) | 0,
             u32: (p: Uint8Array, o: number) => (p[o] | (p[o + 1] << 8) | (p[o + 2] << 16) | (p[o + 3] << 24)) >>> 0 };

export class CC {
  t: Telem = {};
  private ws: WebSocket | null = null;
  private parser = new MspParser();
  private closed = false;

  // Connect to the flightbox hub's MSP-proxy WebSocket (ws://.../msp) on the single HTTP port. The hub
  // fans iNav's MSP downlink to every such CC and forwards each CC's frames up to iNav — one iNav link,
  // many command centers, all over port 8080. (The WASM CC uses /ws for structured telemetry instead.)
  constructor(private url = process.env.CC_URL ?? 'ws://127.0.0.1:8080/msp') {}

  async connect(timeoutMs = 40000): Promise<void> {
    const end = Date.now() + timeoutMs;
    for (;;) {
      try { await this.dial(); break; }
      catch { if (Date.now() > end) throw new Error(`flightbox hub not reachable at ${this.url}`);
              await sleep(250); }
    }
  }

  private dial(): Promise<void> {
    return new Promise((res, rej) => {
      const ws = new WebSocket(this.url);
      ws.binaryType = 'nodebuffer';
      ws.on('open', () => { this.ws = ws; res(); });
      ws.on('message', (d: Uint8Array) => { for (const f of this.parser.push(d)) this.decode(f); });
      ws.on('close', () => { if (!this.closed) this.reconnect(); });
      ws.on('error', (e: unknown) => rej(e));                 // pre-open: fails the dial; post-open: -> close
    });
  }

  private async reconnect() {
    this.ws = null; this.parser = new MspParser();
    try { await this.connect(10000); } catch { /* hub gone; caller times out on telemetry */ }
  }

  private decode(f: { cmd: number; payload: Uint8Array }) {
    const p = f.payload, t = this.t;
    switch (f.cmd) {
      case 0x2000: if (p.length >= 13) { const af = rd.u32(p, 9); t.armed = !!(af & 0x4); t.calibrating = !!(af & (1 << 9)); } break;
      case 0x2220: if (p.length >= 6) { t.roll = rd.i16(p, 0) / 10; t.pitch = rd.i16(p, 2) / 10; t.yaw = ((rd.i16(p, 4) / 10) % 360 + 360) % 360; } break;
      case 106:    if (p.length >= 16) { t.fix = p[0]; t.sats = p[1]; t.lat = rd.i32(p, 2) / 1e7; t.lon = rd.i32(p, 6) / 1e7; t.altAsl = rd.i16(p, 10); t.gs = rd.u16(p, 12) / 100; t.course = rd.u16(p, 14) / 10; } break;
      case 109:    if (p.length >= 6) { t.aglM = rd.i32(p, 0) / 100; t.vs = rd.i16(p, 4) / 100; } break;
      case 121:    if (p.length >= 5) { t.navState = p[1]; t.navWp = p[3]; } break;
    }
    t.updated = Date.now();
  }

  send(cmd: number, payload: Uint8Array = new Uint8Array(0)) { this.ws?.send(encode(cmd, payload)); }

  rc(ch: number[]) { const b = new Uint8Array(ch.length * 2); const dv = new DataView(b.buffer); ch.forEach((v, i) => dv.setUint16(i * 2, v, true)); this.send(200, b); }

  /** Upload NAV waypoints (MSP_SET_WP, action 1). Optional `land`: a safehome at the runway threshold + an
   *  FW approach (runway heading) so a later RTH lands runway-aligned — iNav's fwAutolandApproach. */
  uploadMission(wps: { lat: number; lon: number; altRel: number }[],
                land?: { lat: number; lon: number; heading: number; approachAlt: number; landAlt: number }) {
    wps.forEach((w, i) => {
      const b = new Uint8Array(21); const dv = new DataView(b.buffer);
      b[0] = i + 1; b[1] = 1;
      dv.setInt32(2, Math.round(w.lat * 1e7), true); dv.setInt32(6, Math.round(w.lon * 1e7), true);
      dv.setInt32(10, Math.round(w.altRel * 100), true); b[20] = i === wps.length - 1 ? 0xa5 : 0;
      this.send(209, b);
    });
    if (land) {
      const sh = new Uint8Array(10); const sv = new DataView(sh.buffer);
      sh[0] = 0; sh[1] = 1; sv.setInt32(2, Math.round(land.lat * 1e7), true); sv.setInt32(6, Math.round(land.lon * 1e7), true);
      this.send(0x2039, sh);                                        // MSP2_INAV_SET_SAFEHOME
      const fa = new Uint8Array(15); const fv = new DataView(fa.buffer);
      fa[0] = 0; fv.setInt32(1, Math.round(land.approachAlt * 100), true); fv.setInt32(5, Math.round(land.landAlt * 100), true);
      fa[9] = 0; fv.setInt16(10, Math.round(land.heading), true); fv.setInt16(12, 0, true); fa[14] = 0;
      this.send(0x204b, fa);                                        // MSP2_INAV_SET_FW_APPROACH
    }
  }

  close() { this.closed = true; this.ws?.close(); }
}

export const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));
