"""Publish a fixed local transport snapshot and optionally serve it on port 8767."""

import argparse
import json
import tempfile
from functools import partial
from http.server import ThreadingHTTPServer
from pathlib import Path

from showcase_publish import publish
from showcase_server import ShowcaseHandler

HERE = Path(__file__).resolve().parent
DEFAULT_ROOT = Path(tempfile.gettempdir()) / "outshine-transport-showcase"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--data-root", type=Path)
    parser.add_argument("--catalog", type=Path, default=HERE / "showcase_catalog.json")
    parser.add_argument("--render-tiles", action="store_true")
    parser.add_argument("--publish", action="store_true")
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8767)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Port muss zwischen 1 und 65535 liegen")
    if not (args.publish or args.serve or args.render_tiles):
        parser.error("--publish, --serve oder --render-tiles angeben")
    if (args.publish or args.render_tiles) and args.data_root is None:
        parser.error("--data-root mit den geprüften Transportbildern angeben")
    if args.render_tiles:
        from whole_tile_showcase import main as render_tiles

        render_tiles(data_root=args.data_root.resolve())
    if args.publish or args.render_tiles:
        print(
            json.dumps(
                publish(args.root, args.catalog, source_root=args.data_root.resolve()),
                ensure_ascii=False,
            ),
            flush=True,
        )
    if args.serve:
        root = args.root.resolve()
        if not (root / "manifest.json").is_file():
            parser.error("Zuerst --publish verwenden")
        handler = partial(ShowcaseHandler, directory=str(root))
        with ThreadingHTTPServer((args.host, args.port), handler) as server:
            print(f"Transport-Showcase: http://{args.host}:{args.port}/", flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass


if __name__ == "__main__":
    main()
