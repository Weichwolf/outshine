"""Prepare candidate numeric terrain products; these bytes are not yet admitted."""

import argparse
import json
from pathlib import Path

from camera_height_inputs import digest
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_prepared_surface_archive import FORMAT, pack_surface
from stair_terrain_experiment import frozen


def prepare(args):
    commit = frozen()
    fresh(args.output)
    report = json.loads((args.resident / "comparison.json").read_text())
    receipt = json.loads((args.resident / "verification.json").read_text())
    if (
        not receipt.get("valid_prepared_snapshot")
        or receipt["products_report_sha256"]
        != digest(args.resident / "comparison.json")
        or report["artifacts"]["resident.json"]
        != digest(args.resident / "resident.json")
    ):
        raise ValueError("original independently verified complete resident required")
    state = load_chain(args.resident)
    view, manifest = state[0], state[4]
    args.output.mkdir()
    variants = {}
    for compressed in (False, True):
        name = "compressed" if compressed else "stored"
        target = args.output / ("surface-" + name + ".npz")
        target.write_bytes(pack_surface(view, compressed=compressed))
        variants[name] = {
            "file": target.name,
            "sha256": digest(target),
            "bytes": target.stat().st_size,
        }
    result = {
        "format": FORMAT,
        "producer_commit": commit,
        "resident": str(args.resident.resolve()),
        "resident_sha256": digest(args.resident / "resident.json"),
        "resident_report_sha256": digest(args.resident / "comparison.json"),
        "resident_receipt_sha256": digest(args.resident / "verification.json"),
        "terrain": str(Path(manifest["terrain"]).resolve()),
        "terrain_sha256": manifest["terrain_sha256"],
        "frame": manifest["frame"],
        "variants": variants,
        "scope": "Candidate bytes from normally validated surfaces. This producer report "
        "does not authorize a fast hit. Independent cold verification must establish "
        "an external reference digest before use; normal unknown-input paths remain strict.",
    }
    write_json(args.output / "products.json", result)
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    prepare(parser.parse_args())
