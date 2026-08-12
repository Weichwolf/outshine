#!/usr/bin/env python3
"""THE ORDER THE TILE ANSWERS COME BACK IN, AS AN INPUT. Every request is forwarded to the tile
server unchanged and its response held back by a delay derived from the path and a seed, so one seed
is one arrival order and the same seed is the same order again. Nothing here is a fixture: the
bytes, the status and the headers are the server's own.

It exists because a gate cannot sample what it does not control -- the host decides the completion
order, and on a warm cache it decided the same one six times running.

  test/world/tile_delay.py --port 8171 --upstream http://localhost:8081 --seed 1 --spread 400

Threaded, because the pool asks with eight threads at once: a serving loop that held one response at
a time would impose a queue instead of an order.
"""
import argparse
import http.server
import socketserver
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# Set by the server that is forwarded to; hop-by-hop headers are this proxy's own business and are
# not passed on (RFC 9110 7.6.1).
HOP_BY_HOP = {"connection", "keep-alive", "proxy-authenticate", "proxy-authorization", "te",
              "trailer", "transfer-encoding", "upgrade"}


def delay_ms(path, seed, spread_ms):
    """FNV-1a over the path, so the delay is a property of WHICH tile is asked for and of the seed --
    a request repeated after a retry must not overtake itself."""
    h = (2166136261 ^ seed) & 0xFFFFFFFF
    for b in path.encode("utf-8", "surrogatepass"):
        h = (h ^ b) & 0xFFFFFFFF
        h = (h * 16777619) & 0xFFFFFFFF
    return h % (spread_ms + 1)


class Proxy(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        self.forward("GET")

    def do_HEAD(self):
        self.forward("HEAD")

    def do_POST(self):
        self.forward("POST")

    def log_message(self, fmt, *args):
        pass

    def forward(self, method):
        length = int(self.headers.get("content-length") or 0)
        body = self.rfile.read(length) if length else None
        headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP_BY_HOP}
        headers["Host"] = self.server.Upstream.netloc
        request = urllib.request.Request(
            urllib.parse.urljoin(self.server.Upstream.geturl(), self.path),
            data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(request) as answer:
                status, out, payload = answer.status, answer.headers.items(), answer.read()
        except urllib.error.HTTPError as refusal:
            status, out, payload = refusal.code, refusal.headers.items(), refusal.read()
        except OSError as unreachable:
            self.send_response(502)
            self.send_header("content-type", "text/plain")
            self.send_header("content-length", "0")
            self.end_headers()
            sys.stderr.write(f"tile_delay: {self.path}: {unreachable}\n")
            return
        time.sleep(delay_ms(self.path, self.server.Seed, self.server.SpreadMs) / 1000.0)
        self.send_response(status)
        for name, value in out:
            if name.lower() not in HOP_BY_HOP and name.lower() != "content-length":
                self.send_header(name, value)
        self.send_header("content-length", str(len(payload)))
        self.end_headers()
        if method != "HEAD":
            self.wfile.write(payload)


class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=8099)
    p.add_argument("--upstream", default="http://localhost:8081")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--spread", type=int, default=400)
    a = p.parse_args()

    server = Server(("127.0.0.1", a.port), Proxy)
    server.Upstream = urllib.parse.urlsplit(a.upstream)
    server.Seed = a.seed
    server.SpreadMs = a.spread
    print(f"tile_delay: :{a.port} -> {a.upstream} seed={a.seed} spread={a.spread}ms", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
