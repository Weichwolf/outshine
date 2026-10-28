"""Replay unlit view asset misses, source-free relighting hits and a required miss."""

import argparse
import hashlib
import json
import sqlite3
import subprocess
from pathlib import Path

from replay_building_radiance import DATA, ROOT, compress, image
from replay_source_view_misses import backup


def run(client, folder, request, success):
    (folder / "input.json").write_text(json.dumps(request, indent=2) + "\n")
    command = [str(client), str(folder), "--radiance-world"]
    (folder / "command.json").write_text(json.dumps(command) + "\n")
    with (folder / "terminal.log").open("w") as log:
        result = subprocess.run(
            command, stdout=log, stderr=subprocess.STDOUT, check=False, timeout=180
        )
    (folder / "terminal.exit").write_text(str(result.returncode) + "\n")
    if (result.returncode == 0) != success:
        raise ValueError(f"unexpected native result: {folder}")
    return (
        json.loads((folder / "radiance-result.json").read_text()) if success else None
    )


def replay(args):
    if args.output.resolve().is_relative_to(ROOT):
        raise ValueError(
            "put private native outputs in system temp, outside the checkout"
        )
    args.output.mkdir(parents=True, exist_ok=False)
    cold, warm, missing = (args.output / name for name in ("cold", "warm", "missing"))
    for folder in (cold, warm, missing):
        folder.mkdir()
    request = json.loads((DATA / "source-world-controls.json").read_text())
    request["source_request"] = json.loads(
        (DATA / "source-plane-request.json").read_text()
    )
    request["source_input"] = str(args.source_input.resolve())
    first = run(args.client.resolve(), cold, request, True)
    if (
        len(first["cases"]) != 8
        or first["field_writes"] != 2
        or sum(case["field_hit"] for case in first["cases"]) != 6
        or len(first["captures"]) != 2
        or first["source_capture"]["form_reads"] != 4
        or first["render_device_creations"] != 1
    ):
        raise ValueError("cold demand must build only the two selected raw views")
    backup(cold / "world-view-assets.sqlite", warm / "world-view-assets.sqlite")
    offline = json.loads(json.dumps(request))
    offline["source_input"] = str(args.output / "absent-source/input.json")
    offline["source_request"]["source_tests"] = 0
    offline["source_request"]["cells"].reverse()
    for query in offline["queries"]:
        query["name"] += "-warm"
    second = run(args.client.resolve(), warm, offline, True)
    if (
        second["field_writes"] != 0
        or not all(case["field_hit"] for case in second["cases"])
        or second["captures"]
        or "source_capture" in second
        or second["atlas_read_bytes"] != 0
        or second["atlas_decodes"] != 0
        or second["render_device_creations"] != 1
    ):
        raise ValueError(
            "fresh warm process must relight stored samples without source work"
        )
    images = []
    for before, after in zip(first["cases"], second["cases"], strict=True):
        if before["field_asset_key"] != after["field_asset_key"]:
            raise ValueError(
                "query name, source path, budget and cell order changed asset identity"
            )
        for ending in (
            "-source-field.f32",
            "-radiance-world.linear",
            "-radiance-world.depth",
            "-radiance-world.rgba",
            "-radiance-world.ss16",
        ):
            a = (cold / (before["query"] + ending)).read_bytes()
            b = (warm / (after["query"] + ending)).read_bytes()
            if a != b:
                raise ValueError(
                    f"source-free image mismatch: {before['query']}{ending}"
                )
            if ending == "-source-field.f32":
                image(
                    cold / (before["query"] + ending),
                    request["source_request"]["pixels"] ** 2,
                )
            elif ending in {"-radiance-world.linear", "-radiance-world.ss16"}:
                image(cold / (before["query"] + ending), 1280 * 720)
            images.append(
                {
                    "file": before["query"] + ending,
                    "sha256": hashlib.sha256(a).hexdigest(),
                }
            )
    database = missing / "world-view-assets.sqlite"
    backup(warm / "world-view-assets.sqlite", database)
    with sqlite3.connect(database) as connection:
        connection.execute(
            "DELETE FROM assets WHERE key=?", (first["cases"][0]["field_asset_key"],)
        )
        before = connection.execute(
            "SELECT key,package FROM assets ORDER BY key"
        ).fetchall()
    run(args.client.resolve(), missing, offline, False)
    with sqlite3.connect(database) as connection:
        if (
            before
            != connection.execute(
                "SELECT key,package FROM assets ORDER BY key"
            ).fetchall()
        ):
            raise ValueError("failed source acquisition changed published assets")
    receipt = {
        "cold_raw_views": 2,
        "warm_source_work": False,
        "warm_writes": 0,
        "missing_required_source_fails": True,
        "raw_images": images,
        "artifacts": {"cold": compress(cold), "warm": compress(warm)},
        "scope": "Fixed native asset/cache/relighting consumer; no general asynchronous World, motion/history or frame budget admission.",
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"cached radiance replay passed: {args.output / 'receipt.json'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--source-input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    replay(parser.parse_args())
