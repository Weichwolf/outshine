"""Serve the published vegetation showcase without automatic reloads."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class ShowcaseHandler(SimpleHTTPRequestHandler):
    def list_directory(self, path):
        self.send_error(404, "Showcase page not found")

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path("/tmp/outshine-vegetation-showcase")
    )
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--bind", default="0.0.0.0")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Port must be between 1 and 65535")
    directory = args.directory.resolve()
    if not (directory / "status.json").is_file():
        parser.error("Publish the showcase with showcase.py before starting the server")
    handler = partial(ShowcaseHandler, directory=str(directory))
    with ThreadingHTTPServer((args.bind, args.port), handler) as server:
        print(f"Showcase: http://localhost:{args.port}/", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
