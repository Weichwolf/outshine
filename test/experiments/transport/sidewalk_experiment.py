"""Bounded physical sidewalk bodies with whole actual curb and ground contacts."""

import argparse
import json
import resource
import subprocess
import time
from collections import Counter
from pathlib import Path

import shapely
from bridge_support_contacts import ContactSurface
from camera_height_inputs import digest
from curb_experiment import builder, save_products
from sidewalk_display import scenes
from sidewalk_geometry import audit
from sidewalk_inputs import inputs
from sidewalk_plan import select

HERE = Path(__file__).resolve().parent


def generate(rows, field, curbs, shapes, context, output):
    source = builder(field)
    products, reports = [], []
    faces = 0
    began = time.perf_counter()
    for row in rows:
        record = {k: v for k, v in row.items() if k != "shape"}
        record.update(
            valid=False, footprint_wkb_hex=shapely.to_wkb(row["shape"], hex=True)
        )
        if row["reason"]:
            pass
        elif len(products) >= 256 or time.perf_counter() - began > 60:
            record["reason"] = "sidewalk_optional_piece_or_time_budget"
        else:
            product, note = source.build(row["shape"], minimum_height_m=0.1199999)
            record.update(dict(note))
            if product is not None:
                try:
                    proof = audit(product, row, curbs, shapes, context)
                    if faces + len(product.body.triangles) > 262144:
                        raise ValueError("sidewalk_whole_face_budget")
                except ValueError as error:
                    record.update(valid=False, reason=str(error))
                else:
                    record.update(
                        valid=True, reason="", audit=proof, product=len(products)
                    )
                    products.append(product)
                    faces += len(product.body.triangles)
        reports.append(record)
    if not products:
        raise ValueError("no valid sidewalk; preserve old curb scene")
    data = save_products(output / "sidewalks.npz", products)
    return (
        data,
        reports,
        {
            "preparation_seconds": source.preparation_seconds,
            "generation_and_audit_seconds": time.perf_counter() - began,
            "accepted": len(products),
            "reasons": dict(Counter(r["reason"] for r in reports if not r["valid"])),
        },
    )


def build(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and clean frozen sidewalk source required")
    args.output.mkdir(parents=True)
    ground, field, before, curbs, shapes, owners, traffic, world, excluded, binding = (
        inputs(args)
    )
    comparison = {}
    plans = {}
    for method in ["combined", "individual"]:
        started = time.perf_counter()
        rows, blocked, outside, work = select(
            shapes, owners, traffic, world, excluded, method
        )
        comparison[method] = dict(work, seconds=time.perf_counter() - started)
        plans[method] = rows
    mask = lambda rows: shapely.union_all([r["shape"] for r in rows])
    difference = (
        mask(plans["combined"]).symmetric_difference(mask(plans["individual"])).area
    )
    if difference > 1e-8:
        raise ValueError("sidewalk dilation alternatives differ physically")
    context = (ContactSurface(ground), traffic, world, blocked, outside)
    bodies, rows, generation = generate(
        plans["combined"], field, curbs, shapes, context, args.output
    )
    records, presentation = scenes(args.output, args.curbs, before, bodies)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": binding,
        "comparison": comparison,
        "alternative_symmetric_difference_m2": difference,
        "candidates": rows,
        "generation": generation,
        "render_inputs": records,
        "presentation": presentation,
        "sidewalks_sha256": digest(args.output / "sidewalks.npz"),
        "area_m2": sum(r["audit"]["area_m2"] for r in rows if r["valid"]),
        "joint_length_m": sum(r["audit"]["joint_length_m"] for r in rows if r["valid"]),
        "vertices": len(bodies["vertices"]),
        "faces": len(bodies["triangles"]),
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Bounded procedural sidewalk bodies at actual curb joints; no continuous routes, building clearance/reservation, native or device claim. Old scene/source use unchanged.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ["generation", "area_m2", "joint_length_m", "faces", "seconds"]
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
        "output",
    ]:
        parser.add_argument("--" + name, type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    build(arguments())
