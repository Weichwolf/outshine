"""Serve only published showcase files; never expose experiment directories."""

from http.server import SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import unquote, urlsplit


class ShowcaseHandler(SimpleHTTPRequestHandler):
    def permitted(self):
        path = unquote(urlsplit(self.path).path)
        if path in ("/", "/index.html", "/manifest.json", "/showcase.css"):
            return True
        parts = Path(path).parts
        root = Path(self.directory).resolve()
        target = (root / path.lstrip("/")).resolve()
        return (
            len(parts) == 3
            and parts[:2] == ("/", "assets")
            and target.suffix == ".png"
            and target.is_relative_to(root / "assets")
        )

    def do_GET(self):
        if self.permitted():
            super().do_GET()
        else:
            self.send_error(404)

    def do_HEAD(self):
        if self.permitted():
            super().do_HEAD()
        else:
            self.send_error(404)

    def list_directory(self, path):
        self.send_error(404)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format, *args):
        if len(args) > 1 and str(args[1]) not in ("200", "304"):
            super().log_message(format, *args)
