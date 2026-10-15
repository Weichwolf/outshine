"""Bounded offline rail hardware on the actual affine Vienna ground, preserving its body."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from ground_surface_showcase import combined
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import RailRecipe
from rail_graph_inputs import chain_recipe
from rail_place_experiment import packed_mesh
from tilted_rail_geometry import build_tilted
from tilted_rail_inputs import affine_pieces, inputs
from tilted_rail_verify import GroundContact, admit, metric_audit
from wien_network_experiment import export_body

HERE = Path(__file__).resolve().parent


def save_piece(path, product):
    np.savez_compressed(
        path,
        vertices=product.rails.vertices,
        triangles=product.rails.triangles,
        sleeper_transforms=product.sleeper_transforms,
        plane_origin=product.plane_origin,
        plane_frame=product.plane_frame,
        local_rail_vertices=product.horizontal.rails.vertices,
    )


def generate(pieces, field, raw, bed, reference):
    products, rows = [], []
    began = time.perf_counter()
    sleeper_count = 0
    for index, piece in enumerate(pieces):
        row = {
            "piece": index,
            "chain": piece["chain"],
            "sources": piece["sources"],
            "xy": piece["xy"].tolist(),
            "plane_face": piece["plane_face"],
            "valid": False,
        }
        try:
            if (
                time.perf_counter() - began > 40
                or len(products) >= 128
                or sleeper_count >= 16384
            ):
                raise ValueError("tilted_rail_time_piece_or_sleeper_budget")
            gauge, basis = chain_recipe(piece["sources"], raw)
            if gauge is None:
                raise ValueError(basis)
            triangle = field.vertices[field.triangles[piece["plane_face"]]]
            product, note = build_tilted(
                piece["xy"], triangle, recipe=RailRecipe(gauge_m=gauge)
            )
            if product is None:
                raise ValueError(note["reason"])
            if sleeper_count + len(product.sleeper_transforms) > 16384:
                raise ValueError("tilted_rail_sleeper_budget")
            shape = shapely.union_all(product.footprints)
            missing = shape.difference(bed).area
            if missing > 1e-7:
                raise ValueError("tilted_rail_outside_selected_finite_bed")
            proof = reference.verify(shape, triangle)
            row.update(
                note,
                valid=True,
                reason="",
                gauge_m=gauge,
                gauge_basis=basis,
                bed_missing_m2=missing,
                actual_contact=proof,
                metrics=metric_audit(product, gauge),
            )
            products.append(product)
            sleeper_count += len(product.sleeper_transforms)
        except (ValueError, shapely.errors.GEOSException) as failure:
            row.update(valid=False, reason=str(failure))
        rows.append(row)
    products, work = admit(products, [r for r in rows if r["valid"]])
    if not products:
        raise ValueError(
            "no valid optional rail hardware; original body remains available"
        )
    return products, rows, dict(work, generation_seconds=time.perf_counter() - began)


def scenes(output, baseline, hardware, network):
    previous = json.loads((baseline / "comparison.json").read_text())
    source = baseline / "after-body.npz"
    if (
        previous["dirty"]
        or previous["network_report_sha256"] != digest(network / "comparison.json")
        or digest(source) != previous["render_inputs"][1]["export"]["sha256"]
    ):
        raise ValueError("clean unchanged pier/network baseline required")
    with np.load(source) as d:
        before = Mesh(d["vertices"], d["triangles"]), d["materials"]
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
        colours=camera["colours"] + [[0.09, 0.11, 0.12, 1.0], [0.28, 0.26, 0.22, 1.0]],
    )
    records = []
    for name, (mesh, slots) in (("before", before), ("after", after)):
        proof = export_body(mesh, slots, output / (name + "-body.npz"))
        row = dict(
            camera,
            **proof,
            id="Wien-tilted-rails-" + name,
            mesh=proof["path"],
            mesh_sha256=proof["sha256"],
            output=str(output / (name + ".png")),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        records.append({"export": proof, "scene": row})
    detail = dict(
        records[1]["scene"],
        profile=[1920, 1080],
        id="Wien-tilted-rails-after-1080",
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(detail, indent=2) + "\n")
    return records


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
    (args.output / "pieces").mkdir()
    candidates, chains, work, raw, ground, road, field, bed, bindings = inputs(
        args.network
    )
    pieces, cuts = affine_pieces(candidates, field)
    reference = GroundContact(ground, road)
    products, rows, admission = generate(pieces, field, raw, bed, reference)
    successful = [r for r in rows if r["valid"]]
    for index, (product, row) in enumerate(zip(products, successful, strict=True)):
        path = args.output / "pieces" / f"piece-{index}.npz"
        save_piece(path, product)
        row.update(product=index, mesh_sha256=digest(path))
    hardware = packed_mesh(products)
    if len(hardware["triangles"]) > 262144:
        raise ValueError("tilted_rail_total_hardware_face_budget")
    np.savez_compressed(args.output / "hardware.npz", **hardware)
    del reference
    records = scenes(args.output, args.baseline, hardware, args.network)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": bindings,
        "chains": chains,
        "affine_cuts": cuts,
        "pieces": rows,
        "work": dict(work, **admission),
        "accepted_pieces": len(products),
        "accepted_length_m": sum(r["centreline_length_m"] for r in successful),
        "sleepers": len(hardware["sleeper_transforms"]),
        "hardware_sha256": digest(args.output / "hardware.npz"),
        "baseline_report_sha256": digest(args.baseline / "comparison.json"),
        "render_inputs": records,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Actual tilted hardware on explicit affine ground pieces only. Original base/source usage unchanged; local switch, land-crossing and affine-crease gaps. No C1 rail trajectory, turnout, native, load or LOD claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "commit",
                    "accepted_pieces",
                    "accepted_length_m",
                    "sleepers",
                    "seconds",
                    "peak_rss_kib",
                )
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    build(args)
