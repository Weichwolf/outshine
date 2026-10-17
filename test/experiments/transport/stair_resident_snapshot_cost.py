"""Equal physical/profile state from a verified chain or prepared snapshot, before work."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from camera_height_inputs import digest
from stair_local_cost import result_hash
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_terrain_experiment import frozen


def state_hash(state):
    view, assets, supports, contacts, manifest = state
    arrays = {}
    for i, part in enumerate(view.patches):
        arrays["terrain-" + str(i)] = {
            "vertices": part.mesh.vertices,
            "faces": part.mesh.triangles,
            "active": view.active_faces[part.patch_id],
            **part.face_data,
        }
    for i, asset in enumerate(assets):
        arrays["road-" + str(i)] = dict(asset.data)
    for i, (body, caps) in enumerate(zip(supports, contacts, strict=True)):
        arrays["support-" + str(i)] = {
            "vertices": body.vertices,
            "faces": body.triangles,
        }
        for j, cap in enumerate(caps):
            arrays[f"support-{i}-cap-{j}"] = {
                "vertices": cap.vertices,
                "faces": cap.triangles,
            }
    metadata = {
        "binding": {
            k: manifest[k]
            for k in (
                "frame",
                "origin_lat_lon",
                "source_ids",
                "support_sha256",
                "catalog_sha256",
                "height_floor_m",
            )
        },
        "patch_ids": [p.patch_id for p in view.patches],
        "assets": [[a.identity, a.source_ids, a.rows] for a in assets],
    }
    sha = hashlib.sha256(json.dumps(metadata, sort_keys=True).encode())
    sha.update(result_hash(arrays).encode())
    return sha.hexdigest()


def run(args):
    commit = frozen()
    for path in (args.output, args.profile):
        if path is not None and (
            path.exists() or not path.resolve().is_relative_to(Path("/tmp"))
        ):
            raise ValueError("fresh explicit private snapshot cost outputs required")
    report = json.loads((args.products / "comparison.json").read_text())
    receipt = json.loads((args.products / "verification.json").read_text())
    if not receipt.get("valid_prepared_snapshot") or receipt[
        "products_report_sha256"
    ] != digest(args.products / "comparison.json"):
        raise ValueError("independently verified prepared snapshot required")
    source = report["inputs"]
    path = Path(source["profile_products"]) / "resident.json"
    if digest(path) != source["profile_manifest_sha256"]:
        raise ValueError("same verified preloaded profile metadata required")
    rows = json.loads(path.read_text())["roads"][0]["rows"]
    with np.load(args.products / "road-0.npz", allow_pickle=False) as probe:
        kind = type(probe)
    getter, work = kind.__getitem__, {"decoded_arrays": 0, "decoded_array_bytes": 0}

    def decoded(source, name):
        value = getter(source, name)
        work["decoded_arrays"] += 1
        work["decoded_array_bytes"] += value.nbytes
        return value

    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=False
        )
    with tracker, patch.object(kind, "__getitem__", decoded):
        began = time.perf_counter()
        if args.mode == "chain":
            state = load_chain(
                Path(source["resident"]), [Path(p) for p in source["prior_modules"]]
            )
            assets = (replace(state[1][0], rows=tuple(rows)), *state[1][1:])
            state = state[0], assets, *state[2:]
        else:
            state = load_chain(args.products)
        seconds = time.perf_counter() - began
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    result = {
        "commit": commit,
        "mode": args.mode,
        "state_sha256": state_hash(state),
        "loading_seconds": seconds,
        "profiled": bool(args.profile),
        "peak_rss_before_result_hash_bytes": rss,
        "work": work,
        "source_count": len(state[4]["source_ids"]),
        "scope": "Same actual roads/metadata, patches/active masks, complete supports/caps and source/frame bindings. "
        "Chain receives the same previously verified profile rows preloaded outside measurement; "
        "snapshot preparation/source recovery/imports/result hashing excluded. IO/hash/decode/trees/patch replay included. "
        "Small NPZ access counters are instrumented in both modes; bytes count materialized input arrays, not all copies/IO. "
        "Representation history/paths/commit excluded from the equal-state hash. No runtime/cache/device budget proof.",
    }
    write_json(args.output, result)
    print(
        json.dumps(
            {k: result[k] for k in ("mode", "state_sha256", "loading_seconds", "work")}
        ),
        flush=True,
    )
    return result


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--mode", choices=("chain", "snapshot"), required=True)
    run(parser.parse_args())
