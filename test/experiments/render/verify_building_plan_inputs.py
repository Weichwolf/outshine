"""Independently compare enriched source metadata with the actual native #800 decoder."""

import argparse
import hashlib
import json
import sqlite3
import tempfile
from collections import Counter
from pathlib import Path

import numpy as np
from building_plan_inputs import read_plans
from export_building_batch_inputs import library, payload
from outshine_ref.geometry.occlusion import convex_patch_umbra
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy


def compare_native(directory):
    compared = 0
    for file in sorted((directory / "versioned-inputs").glob("*/input.json")):
        manifest = json.loads(file.read_text())
        entry = next(
            p for p in manifest["native_packages"] if p["file"] == manifest["index"]
        )
        data = (file.parent / manifest["index"]).read_bytes()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError("native frozen index hash mismatch")
        decoded = read_plans(data)
        native = json.loads((directory / f"{manifest['parent']}.json").read_text())
        if len(decoded["records"]) != len(native["sources"]):
            raise ValueError("source count differs from native product decoder")
        bounds = np.full((len(native["sources"]), 2, 3), np.nan)
        known = np.zeros(len(bounds), dtype=bool)
        for index, (record, summary, source) in enumerate(
            zip(
                decoded["records"], decoded["summaries"], native["sources"], strict=True
            )
        ):
            if (
                source["index"] != index
                or record["source_id"] != (source["kind"], source["id"])
                or record["cell"] != source["cell"]
                or summary["shell_error_m"] != source["shell_error_m"]
            ):
                raise ValueError(
                    "source identity/cell/error differs from native decoder"
                )
            if source["bounds"] is None:
                if summary["envelope"] is not None:
                    raise ValueError("unknown native bound became known")
            else:
                np.testing.assert_array_equal(
                    summary["envelope"],
                    [source["bounds"]["min"], source["bounds"]["max"]],
                )
                bounds[index] = summary["envelope"]
                known[index] = True
            compared += 1
        hierarchy = BoundsHierarchy(bounds, known=known)
        without_proof = hierarchy.select([(convex_patch_umbra(None, None), None)])
        if without_proof.hidden.any():
            raise ValueError(
                "native projectability/error metadata became an opacity proof"
            )
    if not compared:
        raise ValueError("actual native source-decoder comparisons required")
    return compared


def snapshot(cache, output):
    database = sqlite3.connect(cache.resolve().as_uri() + "?mode=ro", uri=True)
    database.execute("BEGIN")
    zstd, rows, counts = library(), [], Counter()
    query = """SELECT a.key,a.parent,a.offset,a.bytes,p.codec,p.native_bytes,p.bytes,p.crc
               FROM assets a JOIN packages p ON a.package=p.key
               WHERE a.kind='buildings' ORDER BY a.key"""
    for row in database.execute(query):
        key, _, data = payload(row, zstd)
        plans = read_plans(data)
        (output / f"{key}.index").write_bytes(data)
        rows.append(
            {
                "key": key,
                "file": f"{key}.index",
                "sha256": hashlib.sha256(data).hexdigest(),
                "sources": len(plans["records"]),
                "anchor_ecef": plans["anchor_ecef"],
            }
        )
        counts["parents"] += 1
        counts["sources"] += len(plans["records"])
        for summary in plans["summaries"]:
            counts["known_envelope"] += summary["envelope"] is not None
            counts["known_shell_error"] += summary["shell_error_m"] is not None
            counts["projectable"] += summary["projectable"]
            counts["native_supported"] += summary["supported"]
    database.close()
    (output / "manifest.json").write_text(
        json.dumps(
            {
                "sources": rows,
                "counts": dict(counts),
                "scope": "Original enriched schema-8 source indexes; no detail meshes and no inferred opacity.",
            },
            indent=2,
        )
        + "\n"
    )
    return dict(counts)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    compared = compare_native(args.native.resolve())
    target.mkdir(parents=True)
    counts = snapshot(args.cache, target)
    report = {
        "native_records_exact": compared,
        "snapshot": counts,
        "source_sha256": {
            p: hashlib.sha256(Path(__file__).with_name(p).read_bytes()).hexdigest()
            for p in ("building_plan_inputs.py", Path(__file__).name)
        },
    }
    (target / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(report["snapshot"] | {"native_records_exact": compared}), flush=True
    )
