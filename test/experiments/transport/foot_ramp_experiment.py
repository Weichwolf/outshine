"""Bounded actual source-driven ramp edits with complete local rollback and exports."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

from bridge_support_contacts import ContactSurface
from camera_height_inputs import digest
from curb_experiment import builder, save_products
from foot_ramp_display import scenes
from foot_ramp_edit import edit
from foot_ramp_inputs import inputs
from foot_ramp_plan import plan
from foot_ramp_sources import source_edges
from outshine_ref.contracts import Mesh
from wien_support_experiment import packed

HERE = Path(__file__).resolve().parent


def context(data):
    source = builder(data["field"])
    ground = ContactSurface(data["ground"])
    upper = ContactSurface(
        Mesh(data["ground"].vertices + [0, 0, 0.12], data["ground"].triangles)
    )
    limits = {
        k: data["traffic"].buffer(width, quad_segs=4)
        for k, width in [("curb", 0.18), ("sidewalk", 2.0)]
    }
    return source, ground, upper, limits


def attempt(row, data, states, prepared, method):
    source, ground, upper, limits = prepared
    return edit(
        row, data, states, source, ground, limits, upper_reference=upper, method=method
    )


def comparison(rows, data, prepared):
    values = {}
    for method in ["contact", "solid"]:
        reports = []
        for row in rows:
            states = {i: [p] for i, p in enumerate(data["products"])}
            began = time.perf_counter()
            try:
                product, changes, proof = attempt(row, data, states, prepared, method)
            except ValueError as error:
                result = {
                    "candidate": row["candidate"],
                    "valid": False,
                    "reason": str(error),
                }
            else:
                result = {
                    "candidate": row["candidate"],
                    "valid": True,
                    "proof": proof,
                    "changed_body_faces": sum(
                        len(p.body.triangles)
                        for members in changes.values()
                        for p in members
                    ),
                    "ramp_faces": len(product.body.triangles),
                }
            reports.append(dict(result, seconds=time.perf_counter() - began))
        values[method] = reports
    return values


def generate(rows, data, prepared, method):
    states = {i: [p] for i, p in enumerate(data["products"])}
    ramps, reports = [], []
    began = time.perf_counter()
    for row in rows:
        record = {k: v for k, v in row.items() if k != "shape"}
        record.update(valid=False, footprint_wkb_hex=row["shape"].wkb_hex)
        if len(ramps) >= 8 or time.perf_counter() - began > 60:
            record["reason"] = "ramp_optional_piece_or_time_budget"
        else:
            try:
                product, changes, proof = attempt(row, data, states, prepared, method)
                if (
                    sum(
                        len(p.body.triangles)
                        for members in changes.values()
                        for p in members
                    )
                    > 262144
                ):
                    raise ValueError("ramp_staged_border_face_budget")
            except ValueError as error:
                record["reason"] = str(error)
            else:
                states.update(changes)
                record.update(valid=True, reason="", product=len(ramps), proof=proof)
                ramps.append(product)
        reports.append(record)
    if not ramps:
        raise ValueError(
            "no whole valid ramp edit; original local states remain available"
        )
    return states, ramps, reports, time.perf_counter() - began


def build(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and clean frozen ramp source required")
    args.output.mkdir(parents=True)
    data = inputs(args)
    feet, roads, source_ledger, source_work = source_edges(args.network)
    rows, plan_ledger, plan_work = plan(
        feet,
        roads,
        data["raised"],
        data["traffic"],
        data["world"],
        data["obstacles"],
        data["field"],
    )
    prepared = context(data)
    methods = comparison(rows, data, prepared)
    states, ramps, reports, seconds = generate(rows, data, prepared, args.method)
    borders = [p for members in states.values() for p in members]
    owners = [
        {"owner": i, "component": c}
        for i, members in states.items()
        for c, _p in enumerate(members)
    ]
    save_products(args.output / "borders.npz", borders)
    ramp_pack = packed([p.body for p in ramps], [(p.lower, p.upper) for p in ramps])
    import numpy as np

    np.savez_compressed(args.output / "ramps.npz", **ramp_pack)
    all_packed = packed(
        [p.body for p in borders] + [p.body for p in ramps],
        [(p.lower_contact, p.upper_contact) for p in borders]
        + [(p.lower, p.upper) for p in ramps],
    )
    renders, presentation = scenes(
        args, data, all_packed, [r for r in reports if r["valid"]]
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": data["binding"],
        "source_work": source_work,
        "source_ledger": source_ledger,
        "plan_work": plan_work,
        "plan_ledger": plan_ledger,
        "comparison": methods,
        "method": args.method,
        "candidates": reports,
        "accepted": len(ramps),
        "border_owners": owners,
        "border_sources": data["owners"],
        "borders_sha256": digest(args.output / "borders.npz"),
        "ramps_sha256": digest(args.output / "ramps.npz"),
        "generation_seconds": seconds,
        "render_inputs": renders,
        "presentation": presentation,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Local source-supported ramp/core/flare edits only, whole actual border/mouth contacts. Failed trials preserve original pieces. No whole route, building/consumer reservation, native or device claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in [
                    "commit",
                    "method",
                    "plan_work",
                    "accepted",
                    "generation_seconds",
                    "seconds",
                    "peak_rss_kib",
                ]
            }
        ),
        flush=True,
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in [
        "network",
        "baseline",
        "supports",
        "reference-root",
        "curbs",
        "sidewalks",
        "output",
    ]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["contact", "solid"], default="contact")
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    build(arguments())
