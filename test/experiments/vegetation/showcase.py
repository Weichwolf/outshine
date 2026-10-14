"""Publish exactly four current, verified whole-tile images; refresh manually."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
EXPERIMENTS = ROOT / "build/experiments/vegetation"


def read_json(path):
    return json.loads(path.read_text())


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def atomic_text(path, text):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(text)
    temporary.replace(path)


def render_entry(entry, output):
    source = (EXPERIMENTS / entry["image"]).resolve()
    source.relative_to(EXPERIMENTS)
    metadata = read_json(source.with_suffix(".json"))
    for kind in ("input_sha256", "implementation_sha256"):
        if not metadata.get(kind):
            raise ValueError(f"Missing current rendering provenance: {source.name}")
        for name, expected in metadata[kind].items():
            path = Path(name) if kind == "input_sha256" else HERE / name
            if not path.is_file() or digest(path) != expected:
                raise ValueError(f"Rendering input changed: {path}")
    with Image.open(source) as image:
        resolution = list(image.size)
    if (
        resolution != metadata["resolution"]
        or resolution[0] < 1920
        or resolution[1] < 1080
    ):
        raise ValueError("Whole-tile renderings require at least 1920x1080")
    sha = digest(source)
    name = f"{source.stem}-{sha[:16]}.png"
    target = output / "images" / name
    if not target.is_file():
        shutil.copyfile(source, target)
    return entry | {
        "url": f"/images/{name}",
        "sha256": sha,
        "resolution": resolution,
        "commit": metadata.get("commit"),
        "source_modified": metadata.get("source_modified"),
    }


def publish(output):
    catalog = read_json(HERE / "showcase_catalog.json")
    if len(catalog["renders"]) != 4:
        raise ValueError("The showcase contains exactly four whole-tile images")
    output.mkdir(parents=True, exist_ok=True)
    (output / "images").mkdir(exist_ok=True)
    entries = [render_entry(e, output) for e in catalog["renders"]]
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    payload = {
        "project": "outshine-vegetation",
        "commit": commit,
        "updated_at": datetime.now(UTC).isoformat(timespec="seconds"),
        "renders": entries,
    }
    html = (HERE / "showcase.html").read_text()
    for name in ("showcase.css", "showcase.js"):
        text = (HERE / name).read_text()
        html = html.replace(
            f'/{name}"', f'/{name}?v={hashlib.sha256(text.encode()).hexdigest()[:16]}"'
        )
        atomic_text(output / name, text)
    atomic_text(output / "index.html", html)
    atomic_text(
        output / "status.json", json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    )
    current = {entry["url"].rsplit("/", 1)[-1] for entry in entries}
    for path in (output / "images").iterdir():
        if path.is_file() and path.name not in current:
            path.unlink()
    for index in range(5):
        page = output / f"lod-{index}"
        if page.is_dir():
            shutil.rmtree(page)
    return payload


def start_server(output, port):
    url = f"http://localhost:{port}/status.json"
    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            if json.load(response).get("project") == "outshine-vegetation":
                return
    except (OSError, ValueError):
        pass
    with Path("/tmp/outshine-vegetation-showcase-server.log").open("a") as log:
        process = subprocess.Popen(
            [
                sys.executable,
                str(HERE / "showcase_server.py"),
                "--port",
                str(port),
                "--directory",
                str(output),
            ],
            stdin=subprocess.DEVNULL,
            stdout=log,
            stderr=log,
            start_new_session=True,
        )
    atomic_text(output / "server.pid", str(process.pid) + "\n")
    for _ in range(30):
        if process.poll() is not None:
            raise RuntimeError("Showcase server exited; inspect its /tmp log")
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                if json.load(response).get("project") == "outshine-vegetation":
                    return
        except (OSError, ValueError):
            time.sleep(0.1)
    raise RuntimeError("Showcase server did not answer")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/outshine-vegetation-showcase")
    )
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--start", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must lie between 1 and 65535")
    output = args.output.resolve()
    payload = publish(output)
    if args.start:
        start_server(output, args.port)
    print(
        f"Showcase: http://localhost:{args.port}/ · {payload['commit'][:12]} · vier Kacheln"
    )


if __name__ == "__main__":
    main()
