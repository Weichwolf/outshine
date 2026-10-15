"""Generate bounded rail hardware from four pinned camera windows, entirely offline."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import outshine_ref.transport.rail_hardware as rail_library
import shapely
from outshine_ref import contracts
from outshine_ref.transport.rail_hardware import RailRecipe
from outshine_ref.transport.rail_hardware import bake_rail_sleepers as bake_sleepers
from outshine_ref.transport.rail_hardware import build_rail_hardware as build_track
from outshine_ref.transport.rail_hardware._geometry import SLEEPER
from rail_graph_inputs import chain_recipe, rail_chains, read_window, safe_pieces
from rail_piece_admission import admit_pieces

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def packed_mesh(products):
    vertices, faces, slots, transforms = [], [], [], []
    offset = 0
    for product in products:
        rails = product.rails
        sleepers = bake_sleepers(product.sleeper_transforms)
        for mesh, material in ((rails, 6), (sleepers, 7)):
            vertices.append(mesh.vertices)
            faces.append(mesh.triangles + offset)
            slots.append(np.full(len(mesh.triangles), material, np.uint8))
            offset += len(mesh.vertices)
        transforms.append(product.sleeper_transforms)
    return {
        "vertices": np.vstack(vertices) if vertices else np.empty((0, 3)),
        "triangles": np.vstack(faces) if faces else np.empty((0, 3), np.uint32),
        "materials": np.concatenate(slots) if slots else np.empty(0, np.uint8),
        "sleeper_transforms": np.concatenate(transforms)
        if transforms
        else np.empty((0, 3, 4)),
    }


def generate_window(name, source_name, source_root, output):
    began = time.perf_counter()
    source, raw, graphs, bed, pins = read_window(source_root, source_name)
    recipes = json.loads((HERE / "reference/place-sources.json").read_text())
    recipe_record = next(r for r in recipes["cases"] if r["place"] == name)
    datum = recipe_record["flat_recipe_datum_m"]
    chains, branches = [], []
    for graph in graphs:
        if graph["rail_graph"]:
            values, points = rail_chains(graph)
            chains.extend(values)
            branches.extend(points)
    pieces, ledger, work = safe_pieces(
        chains,
        np.array(branches).reshape(-1, 2),
        [g for g in graphs if not g["rail_graph"]],
        np.hypot(1.3, 0.12),
    )
    products, results = [], []
    shapely.prepare(bed)
    for index, piece in enumerate(pieces):
        gauge, basis = chain_recipe(piece["sources"], raw)
        row = {
            "piece": index,
            "chain": piece["chain"],
            "sources": piece["sources"],
            "requested_length_m": float(shapely.LineString(piece["xy"]).length),
            "gauge_basis": basis,
        }
        if gauge is None:
            row.update(valid=False, reason=basis)
        elif len(products) >= 2048 or time.perf_counter() - began >= 15:
            row.update(valid=False, reason="rail_window_time_or_piece_budget")
        else:
            product, report = build_track(piece["xy"], datum, RailRecipe(gauge_m=gauge))
            row.update(report)
            if product is not None:
                covered = shapely.covers(bed, product.footprints)
                if not covered.all():
                    row.update(
                        valid=False,
                        reason="rail_footprint_outside_selected_bed",
                        outside_footprints=int(np.count_nonzero(~covered)),
                    )
                else:
                    row["gauge_m"] = gauge
                    row["whole_footprints_checked"] = len(covered)
                    products.append(product)
        results.append(row)
    products, admission = admit_pieces(products, [r for r in results if r["valid"]])
    work.update(admission)
    packed = packed_mesh(products)
    path = output / (name + ".npz")
    np.savez_compressed(path, **packed)
    report = {
        "place": name,
        "origin_lat_lon": source["origin_lat_lon"],
        "horizontal_bed_datum_m": datum,
        "pins": pins,
        "chains": ledger,
        "pieces": results,
        "work": work,
        "accepted_pieces": len(products),
        "input_chains": len(chains),
        "accepted_length_m": sum(
            r["centreline_length_m"] for r in results if r["valid"]
        ),
        "sleeper_instances": len(packed["sleeper_transforms"]),
        "display_vertices": len(packed["vertices"]),
        "display_triangles": len(packed["triangles"]),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "seconds_with_input_output": time.perf_counter() - began,
        "peak_worker_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Hardware on existing horizontal camera-window beds only; retained old C1/flat windows unchanged. Original source usage is not reclassified. Whole footprints covered by chosen pinned domain at its declared datum. No switches, active level crossings, support load, train routing, terrain or native proof.",
    }
    (output / (name + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "place",
                    "input_chains",
                    "accepted_pieces",
                    "accepted_length_m",
                    "sleeper_instances",
                    "seconds_with_input_output",
                )
            }
        ),
        flush=True,
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--camera-products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("fresh empty external output required")
    args.output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    rows = {}
    for name, source in (
        ("Wien", "Vienna"),
        ("Tokyo", "Tokyo"),
        ("CentralPark", "CentralPark"),
        ("Feldkirch", "Feldkirch"),
    ):
        rows[name] = generate_window(name, source, args.camera_products, args.output)
    receipt = {
        "generation_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    "test/experiments/transport",
                ],
                cwd=ROOT,
                text=True,
            ).strip()
        ),
        "cases": rows,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in HERE.glob("rail_*.py")
        },
        "library_sha256": {
            **{
                "rail_hardware/" + p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                for p in Path(rail_library.__file__).parent.glob("*.py")
            },
            "contracts.py": hashlib.sha256(
                Path(contracts.__file__).read_bytes()
            ).hexdigest(),
        },
        "shared_sleeper_sha256": hashlib.sha256(
            SLEEPER.vertices.tobytes() + SLEEPER.triangles.tobytes()
        ).hexdigest(),
    }
    (args.output / "generation.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
