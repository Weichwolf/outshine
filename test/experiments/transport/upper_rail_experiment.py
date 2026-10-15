"""Actual hardware on selected unchanged subway viaduct patches; two bounded methods."""

import argparse
import json
import resource
import subprocess
import time
from collections import Counter
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from ground_surface_showcase import combined
from outshine_ref.contracts import Mesh
from rail_bed_patches import contact_patches, cut_to_patches
from rail_place_experiment import packed_mesh
from retained_surface import surface_domain
from tilted_rail_experiment import generate, save_piece
from upper_rail_contacts import UpperContact
from upper_rail_inputs import inputs
from wien_network_experiment import export_body

HERE = Path(__file__).resolve().parent
BASE_REPORT = "69fb6e3e7fcd8cdcac98642c570d6a2e3f9200d1c7a984a024e73e0918d2cf0b"
BASE_BODY = "79c8d4c23dde0717b26410dbd150368cde55922bb94c1805f8a9b80687044c59"


def read_base(path):
    if (
        digest(path / "comparison.json") != BASE_REPORT
        or digest(path / "after-body.npz") != BASE_BODY
    ):
        raise ValueError("unchanged verified ground-rail/network baseline required")
    with np.load(path / "after-body.npz") as d:
        return Mesh(d["vertices"], d["triangles"]), d["materials"]


def write_scenes(output, baseline, before, hardware):
    after = combined(
        [
            before,
            (
                Mesh(hardware["vertices"], hardware["triangles"]),
                np.where(hardware["materials"] == 6, 8, 9),
            ),
        ]
    )
    camera = json.loads((baseline / "after-scene.json").read_text())
    centre = np.median(hardware["vertices"][:, :2], axis=0)
    shift = np.r_[centre - np.asarray(camera["target"])[:2], 0]
    camera.update(
        eye=(np.asarray(camera["eye"]) + shift).tolist(),
        target=(np.asarray(camera["target"]) + shift).tolist(),
        span_m=1400,
    )
    rows = []
    for name, (mesh, slots) in (("before", before), ("after", after)):
        proof = export_body(mesh, slots, output / (name + "-body.npz"))
        row = dict(
            camera,
            **proof,
            id="Wien-upper-rails-" + name,
            mesh=proof["path"],
            mesh_sha256=proof["sha256"],
            output=str(output / (name + ".png")),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        rows.append({"export": proof, "scene": row})
    high = dict(
        rows[1]["scene"],
        profile=[1920, 1080],
        id="Wien-upper-rails-after-1080",
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(high, indent=2) + "\n")
    return rows


def build(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and clean frozen source required")
    args.output.mkdir(parents=True)
    before = read_base(args.baseline)
    deck, candidates, raw, chains, work, binding = inputs(args.reference)
    patches, preparation = contact_patches(deck, method=args.method)
    pieces, cuts, cutting = cut_to_patches(candidates, patches)
    bed = surface_domain(deck)
    contact = UpperContact(deck, before[0])
    products, rows, admission = generate(
        pieces, deck, raw, bed, contact, max_pieces=512
    )
    base_tests = contact.base_tests
    del contact
    successful = [r for r in rows if r["valid"]]
    (args.output / "pieces").mkdir()
    for index, (product, row) in enumerate(zip(products, successful, strict=True)):
        path = args.output / "pieces" / f"piece-{index}.npz"
        save_piece(path, product)
        row.update(product=index, mesh_sha256=digest(path))
    hardware = packed_mesh(products)
    if len(hardware["triangles"]) > 262144:
        raise ValueError("upper rail whole hardware face budget")
    np.savez_compressed(args.output / "hardware.npz", **hardware)
    records = write_scenes(args.output, args.baseline, before, hardware)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "method": args.method,
        "bindings": binding,
        "baseline_report_sha256": BASE_REPORT,
        "baseline_body_sha256": BASE_BODY,
        "patches": [
            {"plane_face": p["plane_face"], "faces": p["faces"]} for p in patches
        ],
        "chains": chains,
        "affine_cuts": cuts,
        "pieces": rows,
        "work": dict(
            work,
            **cutting,
            **admission,
            preparation=preparation,
            baseline_triangle_tests=base_tests,
        ),
        "omitted_reasons": dict(Counter(r["reason"] for r in rows if not r["valid"])),
        "accepted_pieces": len(products),
        "accepted_length_m": sum(r["centreline_length_m"] for r in successful),
        "candidate_length_xy_m": sum(r["candidate_length_m"] for r in cuts),
        "sleepers": len(hardware["sleeper_transforms"]),
        "hardware_sha256": digest(args.output / "hardware.npz"),
        "render_inputs": records,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Selected explicit upper subway axes, finite affine deck patches. Old bodies/source usage unchanged. Local switch/patch/end gaps; no joined train route, smoothing, turnout, native or LOD acceptance.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in [
                    "commit",
                    "method",
                    "accepted_pieces",
                    "accepted_length_m",
                    "sleepers",
                    "omitted_reasons",
                    "seconds",
                    "peak_rss_kib",
                ]
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["reference", "baseline", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["triangle", "patch"], required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    build(args)
