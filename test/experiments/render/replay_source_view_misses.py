"""Replay original source-view misses and a fresh input-free WorldCandidate hit."""

import argparse
import hashlib
import json
import sqlite3
import subprocess
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-full-view"


def backup(source, destination):
    with (
        sqlite3.connect(source.resolve().as_uri() + "?mode=ro", uri=True) as original,
        sqlite3.connect(destination) as copied,
    ):
        original.backup(copied)


def run(client, directory, request, success):
    (directory / "input.json").write_text(json.dumps(request, indent=2) + "\n")
    command = [str(client), str(directory), "--cached-world"]
    (directory / "command.json").write_text(json.dumps(command) + "\n")
    with (directory / "terminal.log").open("w") as log:
        result = subprocess.run(
            command, check=False, stdout=log, stderr=subprocess.STDOUT, timeout=180
        )
    (directory / "terminal.exit").write_text(str(result.returncode) + "\n")
    if (result.returncode == 0) != success:
        raise ValueError(f"unexpected native result: {directory}")
    return (
        json.loads((directory / "world-result.json").read_text()) if success else None
    )


def compare(cold, warm, queries):
    hashes = []
    for query in queries:
        for suffix in ("linear", "depth", "rgba", "ss16"):
            name = query["name"] + "-cached-world." + suffix
            original = (cold / name).read_bytes()
            replay = (
                warm / name.replace("-cached-world", "-replay-cached-world")
            ).read_bytes()
            if original != replay:
                raise ValueError(f"cold/warm image mismatch: {name}")
            if suffix == "ss16":
                image = np.frombuffer(original, dtype="<f4").reshape(720, 1280, 4)
                if not np.isfinite(image).all() or image[:, :, :3].sum() <= 0:
                    raise ValueError(
                        "native image must contain finite nonempty source colour"
                    )
            hashes.append(
                {"file": name, "sha256": hashlib.sha256(original).hexdigest()}
            )
    return hashes


def replay(args):
    if args.output.resolve().is_relative_to(ROOT):
        raise ValueError(
            "put private native outputs in system temp, outside the checkout"
        )
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads((DATA / "source-world-controls.json").read_text())
    request["source_request"] = json.loads(
        (DATA / "source-plane-request.json").read_text()
    )
    request["source_input"] = str(args.source_input.resolve())
    cold, warm, missing = (args.output / name for name in ("cold", "warm", "missing"))
    for directory in (cold, warm, missing):
        directory.mkdir()
    first = run(args.client, cold, request, True)
    source = first["source_capture"]
    if (
        len(first["cases"]) != 8
        or first["writes"] != 4
        or sum(c["hit"] for c in first["cases"]) != 4
        or len(first["captures"]) != 2
        or source["form_reads"] != 4
        or source["device_creations"] != 2
        or sum(w["rays"] for c in first["captures"] for w in c["work"]) != 32768
    ):
        raise ValueError("cold source-view production differs from the fixed control")
    backup(cold / "world-view-assets.sqlite", warm / "world-view-assets.sqlite")
    offline = json.loads(json.dumps(request))
    offline["source_input"] = str(args.output / "absent-source/input.json")
    offline["source_request"]["source_tests"] = 0
    offline["source_request"]["cells"].reverse()
    for query in offline["queries"]:
        query["name"] += "-replay"
    hit = run(args.client, warm, offline, True)
    if (
        hit["source_read_bytes"] != 0
        or hit["source_decodes"] != 0
        or hit["writes"] != 0
        or hit["captures"]
        or "source_capture" in hit
        or not all(c["hit"] for c in hit["cases"])
        or [c["asset_key"] for c in first["cases"]]
        != [c["asset_key"] for c in hit["cases"]]
    ):
        raise ValueError("a fresh warm process must not acquire or produce source data")
    hashes = compare(cold, warm, request["queries"])
    database = missing / "world-view-assets.sqlite"
    backup(cold / "world-view-assets.sqlite", database)
    with sqlite3.connect(database) as connection:
        connection.execute(
            "DELETE FROM assets WHERE key=?", (first["cases"][0]["asset_key"],)
        )
        before = connection.execute(
            "SELECT key,package FROM assets ORDER BY key"
        ).fetchall()
    run(args.client, missing, offline, False)
    with sqlite3.connect(database) as connection:
        if (
            before
            != connection.execute(
                "SELECT key,package FROM assets ORDER BY key"
            ).fetchall()
        ):
            raise ValueError("failed source acquisition changed published assets")
    report = {
        "raw_image_hashes": hashes,
        "warm_source_bytes": 0,
        "warm_source_decodes": 0,
        "warm_writes": 0,
        "missing_required_source_fails": True,
        "scope": "Controlled source production/cache/WorldCandidate. No original-image "
        "quality, general World, asynchronous runtime scheduling, SSE or device-budget admission.",
    }
    (args.output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"source misses and fresh offline replay passed: {args.output / 'receipt.json'}"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--source-input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    replay(parser.parse_args())
