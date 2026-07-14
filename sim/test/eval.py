#!/usr/bin/env python3
"""FlightBox headless evaluation harness.

Connects to the flightbox exactly like the command center (WebSocket on :8080):
reads telemetry, sends control (gamepad-equivalent), and asserts the Teil-A
behaviours end-to-end through the real chain (iNav SITL <-> xp_bridge <-> flightbox).

Usage:  python3 sim/test/eval.py            # assumes containers running on :8080
        HOST=1.2.3.4:8080 python3 ...
Exit code 0 = all PASS.
"""
import socket, struct, base64, os, sys, time, threading

HOST = os.environ.get("HOST", "127.0.0.1:8080")
IP, PORT = HOST.split(":")[0], int(HOST.split(":")[1])
TELE = struct.Struct("<IfffffffffffBBH")           # protocol.h telem_packet_t
CTRL = struct.Struct("<IffffBBH")                   # ctrl_packet_t
MAG_TELE, MAG_VID, MAG_CTRL = 0x314D4C54, 0x31444956, 0x314C5443
STATES = ["DISARM", "ARMED", "CLIMB", "LOITER", "MANUAL", "RTH"]

class CC:
    """Minimal command-center client: WS to the flightbox."""
    def __init__(self):
        k = base64.b64encode(os.urandom(16)).decode()
        self.s = socket.create_connection((IP, PORT), timeout=4)
        self.s.sendall(("GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
                        "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n" % k).encode())
        r = b""
        while b"\r\n\r\n" not in r:
            r += self.s.recv(1024)
        assert b"101" in r, "WS handshake failed"
        self.s.setblocking(False)
        self.buf = bytearray()
        self.telem = None
        self.vids = 0

    def _frames(self):
        try:
            while True:
                d = self.s.recv(65536)
                if not d: break
                self.buf += d
        except BlockingIOError:
            pass
        out = []
        while len(self.buf) >= 2:
            b1 = self.buf[1] & 0x7f; off = 2
            if b1 == 126:
                if len(self.buf) < 4: break
                ln = struct.unpack_from(">H", self.buf, 2)[0]; off = 4
            elif b1 == 127:
                if len(self.buf) < 10: break
                ln = struct.unpack_from(">Q", self.buf, 2)[0]; off = 10
            else:
                ln = b1
            if len(self.buf) < off + ln: break
            out.append(bytes(self.buf[off:off+ln])); del self.buf[:off+ln]
        return out

    def poll(self):
        for f in self._frames():
            if len(f) < 4: continue
            mg = struct.unpack_from("<I", f)[0]
            if mg == MAG_TELE and len(f) == TELE.size:
                self.telem = TELE.unpack(f)
            elif mg == MAG_VID:
                self.vids += 1

    def send(self, roll=0, pitch=0, yaw=0, thr=0.5, arm=1, link=1):
        pl = CTRL.pack(MAG_CTRL, roll, pitch, yaw, thr, arm, link, 0)
        m = os.urandom(4)
        self.s.sendall(bytes([0x82, 0x80 | len(pl)]) + m +
                       bytes(b ^ m[i & 3] for i, b in enumerate(pl)))

    def wait(self, secs, **ctrl):
        """Drive control for `secs`, return list of telem samples."""
        samples = []; t0 = time.time()
        while time.time() - t0 < secs:
            if ctrl: self.send(**ctrl)
            self.poll()
            if self.telem: samples.append(self.telem)
            time.sleep(0.05)
        return samples

def T(m): return dict(state=STATES[m[12] % 6], roll=m[1], pitch=m[2], yaw=m[3],
                      alt=m[4], gs=m[7], batt=m[8], home=m[9], hbrg=m[10], rssi=m[13])

def main():
    results = []
    def check(name, ok, detail=""):
        results.append((name, ok)); print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, detail))

    print("== FlightBox headless eval (via flightbox %s) ==" % HOST)

    # 0) flightbox HTTP/WASM subsystem
    def http_get(path):
        c = socket.create_connection((IP, PORT), timeout=4)
        c.sendall(("GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n" % path).encode())
        d = b""
        try:
            while True:
                b = c.recv(65536)
                if not b: break
                d += b
        except Exception: pass
        c.close(); return d
    idx = http_get("/"); wasm = http_get("/cc.wasm")
    check("HTTP serves index", b"200" in idx.split(b"\r\n", 1)[0] and b"text/html" in idx)
    check("HTTP serves WASM (mime)", b"200" in wasm.split(b"\r\n", 1)[0] and b"application/wasm" in wasm)

    cc = CC()

    # 1) telemetry + video pipeline
    s = cc.wait(3)
    check("telemetry received", cc.telem is not None)
    check("video stream", cc.vids > 10, "frames=%d" % cc.vids)
    if cc.telem is None:
        print("no telemetry -> abort"); return 1

    # 2) autonomous launch: armed + airborne within 25s
    armed_at = None; flying_at = None; t0 = time.time()
    while time.time() - t0 < 25 and not (armed_at and flying_at):
        cc.poll()
        if cc.telem:
            m = T(cc.telem)
            if armed_at is None and m["rssi"] > 0: armed_at = time.time() - t0
            if flying_at is None and m["gs"] > 5: flying_at = time.time() - t0
        time.sleep(0.1)
    check("auto-arm (senderless)", armed_at is not None, "t=%.1fs" % (armed_at or -1))
    check("airborne (gs>5)", flying_at is not None, "t=%.1fs" % (flying_at or -1))

    # 3) telemetry sanity (no FDM blowup)
    m = T(cc.telem)
    check("telemetry sane", m["home"] < 20000 and 5 < m["batt"] < 20 and abs(m["roll"]) < 90,
          "home=%.0fm batt=%.1fV roll=%.1f" % (m["home"], m["batt"], m["roll"]))

    # 4) control response: command roll right, expect bank; then left
    base = T(cc.telem)["roll"]
    sr = cc.wait(3.0, roll=0.8, thr=0.6); rr = T(sr[-1])["roll"] if sr else base
    sl = cc.wait(3.0, roll=-0.8, thr=0.6); rl = T(sl[-1])["roll"] if sl else base
    check("roll control responds", (rr - rl) > 8, "right=%.1f left=%.1f" % (rr, rl))

    # 5) RC-loss -> autonomous RTH loiter: cut the operator link and verify the aircraft
    #    switches to RTH state (rssi=0) and keeps loitering near home — bounded within the
    #    envelope and maintaining altitude (no flyaway, no dive into the ground).
    fs = cc.wait(30.0, arm=1, link=0)
    sm = [T(x) for x in fs]
    rth   = any(x["state"] == "RTH" for x in sm)
    rssi0 = any(x["rssi"] == 0 for x in sm)
    homes = [x["home"] for x in sm]; alts = [x["alt"] for x in sm]
    bounded  = homes and max(homes) < 1400               # stays within the loiter/video envelope
    airborne = alts and min(alts) > 40                   # holds altitude (no crash)
    check("RC-loss -> autonomous RTH", rth and rssi0)
    check("RTH loiters over home (bounded, airborne)", bool(bounded and airborne),
          "home<=%.0fm  alt>=%.0fm" % (max(homes) if homes else -1, min(alts) if alts else -1))

    npass = sum(1 for _, ok in results if ok)
    print("== %d/%d PASS ==" % (npass, len(results)))
    return 0 if npass == len(results) else 1

if __name__ == "__main__":
    sys.exit(main())
