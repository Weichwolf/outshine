"""Build one complete conforming soil/transport inspection scene from pinned products."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import (
    digest,
    fixed_bed_facets,
    lumped_weights,
    rail_footprints,
)
from capture_height_targets import sample_targets
from ground_surface import ground_layout
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, close_surface
from outshine_ref.transport.weighted_heights import fit_weighted_heights
from rail_place_display import append_hardware
from surface_height_frames import frames_for_surface
from surface_solid_showcase import inspection_view
from transport_place_shots import flat_model

HERE = Path(__file__).resolve().parent


def prepared(name, root, rails, sources):
    capture = json.loads((root / "capture.json").read_text())["cases"][name]
    source = capture["source"]
    raw = json.loads((root / "inputs" / (source + "-input.json")).read_text())
    datum = capture["datum_m"]
    top, slots, proof = flat_model(source, None, {}, datum, camera_products=root)
    xy, road_faces = top.vertices[:, :2], top.triangles
    domain = shapely.box(*(xy.min(axis=0) - 4), *(xy.max(axis=0) + 4))
    layout, ground = ground_layout(xy, road_faces, domain, max_faces=262144)
    ledger = json.loads(
        (root / "results" / source / "components/source-usage.json").read_text()
    )
    used = {r["id"] for r in ledger if r["status"] != "not_used"}
    frames, frame_proof = frames_for_surface(xy, road_faces, slots, raw, used)
    frames = np.concatenate(
        (
            frames,
            np.broadcast_to(
                np.eye(2) / (2 / 3), (len(layout["faces"]) - len(road_faces), 2, 2)
            ),
        )
    )
    targets, receipts = sample_targets(
        raw["origin_lat_lon"], layout["xy"], sources, 11, fetch=True
    )
    footprints, rail_datum, binding = rail_footprints(
        rails, name, digest(root / "generation.json")
    )
    if rail_datum != datum:
        raise ValueError("hardware and selected reference datum differ")
    fixed, contacts = fixed_bed_facets(layout["xy"], layout["faces"], footprints)
    arrays = {
        "xy": layout["xy"],
        "faces": layout["faces"],
        "targets": targets,
        "frames": frames,
        "fixed": fixed,
        "weights": lumped_weights(layout["xy"], layout["faces"]),
        "roles": np.r_[slots, np.full(len(layout["faces"]) - len(road_faces), 5)],
    }
    meta = {
        "place": name,
        "source_binding": {"generation_sha256": digest(root / "generation.json")},
        "rail_binding": binding,
        "fixed_datum_m": datum,
        "origin_lat_lon": raw["origin_lat_lon"],
        "dem_receipts": receipts,
        "contacts": contacts,
        "ground": ground,
        "frame_recipe": frame_proof,
        "flat_product": proof,
    }
    return arrays, meta


def build(name, root, rails, sources, output):
    began = time.perf_counter()
    arrays, meta = prepared(name, root, rails, sources)
    output.mkdir(parents=True, exist_ok=True)
    input_path = output / (name + "-input.npz")
    np.savez_compressed(input_path, **arrays)
    meta["input_sha256"] = digest(input_path)
    (output / (name + "-input.json")).write_text(json.dumps(meta, indent=2) + "\n")
    prior = Mesh(np.c_[arrays["xy"], arrays["targets"]], arrays["faces"])
    result = fit_weighted_heights(
        prior,
        arrays["frames"],
        weights=arrays["weights"],
        reference_datum_m=meta["fixed_datum_m"],
        fixed_vertices=arrays["fixed"],
        budget=HeightBudget(time_limit_s=8, max_vertices=262144, max_faces=262144),
    )
    if result.mesh is None:
        raise ValueError("gallery height fitting absent: " + str(result.report))
    fitted = output / (name + "-least_squares.npz")
    np.savez_compressed(
        fitted,
        xy=arrays["xy"],
        faces=arrays["faces"],
        heights=result.mesh.vertices[:, 2],
    )
    report = {
        "valid": True,
        "input_sha256": digest(input_path),
        "mesh_sha256": digest(fitted),
        "fit": result.report,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
            ).strip()
        ),
    }
    (output / (name + "-least_squares.json")).write_text(
        json.dumps(report, indent=2) + "\n"
    )
    body = close_surface(result.mesh, 1.0)
    slots = np.r_[
        arrays["roles"], np.full(len(body.triangles) - len(arrays["faces"]), 2)
    ]
    body, slots, hardware = append_hardware(
        body,
        slots,
        rails,
        name,
        meta["origin_lat_lon"],
        meta["fixed_datum_m"],
        digest(root / "generation.json"),
    )
    mesh = output / (name + ".npz")
    np.savez_compressed(
        mesh, vertices=body.vertices, triangles=body.triangles, materials=slots
    )
    spec = json.loads((HERE / "reference/transport-gallery.json").read_text())
    case = next(r for r in spec["cases"] if r["id"] == name)
    view = inspection_view(body.vertices, eye_direction=case["eye_direction"])
    scene = dict(
        id=name,
        mesh=str(mesh),
        mesh_sha256=digest(mesh),
        output=str(output / (name + ".png")),
        background=[0.82, 0.85, 0.85],
        **view,
    )
    (output / (name + "-scene.json")).write_text(json.dumps([scene], indent=2) + "\n")
    proof = {
        "input_sha256": digest(input_path),
        "mesh_sha256": digest(mesh),
        "hardware": hardware,
        "seconds": time.perf_counter() - began,
        "scene": scene,
        "scope": "Complete conforming procedural soil and retained ground transport; same weighted-height/rail kernels. No elevated decks, underground routes, switches, platforms, buildings, traffic or native runtime claim.",
    }
    (output / (name + "-scene-proof.json")).write_text(
        json.dumps(proof, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "place": name,
                "seconds": proof["seconds"],
                "vertices": len(body.vertices),
                "faces": len(body.triangles),
                "fit": result.report,
            }
        ),
        flush=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--flat", type=Path, required=True)
    parser.add_argument("--rails", type=Path, required=True)
    parser.add_argument("--dem-sources", type=Path, required=True)
    parser.add_argument("--place", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    build(args.place, args.flat, args.rails, args.dem_sources, args.output)


if __name__ == "__main__":
    main()
