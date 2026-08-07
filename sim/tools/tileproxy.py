#!/usr/bin/env python3
"""A recording / replaying proxy in front of fb-tiles, so a render can be made hermetic.

  tools/tileproxy.py --port 8099 --record build/tilecorpus          # forwards to :8081, keeps bodies
  tools/tileproxy.py --port 8099 --replay build/tilecorpus          # answers only from the corpus

Point the client at it with --base http://127.0.0.1:PORT. In record mode every distinct path is
fetched once and then answered from memory, so the corpus is a snapshot of one moment of the server.
In replay mode nothing leaves the machine: if two renders against the SAME corpus still differ, the
server is not the cause.

manifest.json maps path -> {status, len, md5}; a corpus recorded twice can therefore be diffed to see
whether the server's answers themselves moved.
"""

import argparse
import hashlib
import http.server
import json
import os
import threading
import urllib.request

ARGS = None
CORPUS = {}
LOCK = threading.Lock()


def key(path):
    return hashlib.sha1(path.encode()).hexdigest()


class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        p = self.path
        with LOCK:
            hit = CORPUS.get(p)
        if hit is None:
            if ARGS.replay:
                self.send_response(404)
                self.send_header("Content-Length", "0")
                self.end_headers()
                with LOCK:
                    CORPUS[p] = (404, b"", "application/octet-stream")
                    MISSES.append(p)
                return
            try:
                with urllib.request.urlopen(ARGS.upstream + p, timeout=60) as r:
                    body = r.read()
                    hit = (r.status, body, r.headers.get("Content-Type", "application/octet-stream"))
            except urllib.error.HTTPError as e:
                hit = (e.code, e.read(), e.headers.get("Content-Type", "application/octet-stream"))
            except Exception as e:
                hit = (599, str(e).encode(), "text/plain")
            with LOCK:
                CORPUS[p] = hit
                if ARGS.record:
                    open(os.path.join(ARGS.record, key(p)), "wb").write(hit[1])
                    MANIFEST[p] = {"status": hit[0], "len": len(hit[1]),
                                   "md5": hashlib.md5(hit[1]).hexdigest(), "ct": hit[2]}
                    open(os.path.join(ARGS.record, "manifest.json"), "w").write(
                        json.dumps(MANIFEST, indent=1, sort_keys=True))
        st, body, ct = hit
        self.send_response(st)
        self.send_header("Content-Type", ct)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)


MANIFEST = {}
MISSES = []


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    global ARGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--upstream", default="http://127.0.0.1:8081")
    ap.add_argument("--record")
    ap.add_argument("--replay")
    ARGS = ap.parse_args()
    d = ARGS.record or ARGS.replay
    if not d:
        ap.error("--record or --replay")
    os.makedirs(d, exist_ok=True)
    if ARGS.replay:
        man = json.load(open(os.path.join(d, "manifest.json")))
        for p, m in man.items():
            body = open(os.path.join(d, key(p)), "rb").read()
            CORPUS[p] = (m["status"], body, m["ct"])
        print("replay: %d paths" % len(CORPUS), flush=True)
    else:
        MANIFEST.clear()
    print("listening on %d" % ARGS.port, flush=True)
    Server(("127.0.0.1", ARGS.port), H).serve_forever()


if __name__ == "__main__":
    main()
