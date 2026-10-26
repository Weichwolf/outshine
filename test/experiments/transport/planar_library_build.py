"""Fresh source/wheel construction on all actual already admitted real rail pieces."""

import argparse
import inspect
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.transport.rail_hardware import RailRecipe, build_planar_rail_hardware
from planar_library_inputs import cases, original
from rail_place_experiment import packed_mesh
from tilted_rail_experiment import save_piece
from transport_surface_experiment import camera_settings

HERE = Path(__file__).resolve().parent
LATEST_BODY = "3e272f12fe5ca9f9e6bd8026761e7320280c678e6838a0f4dde83cb372d15b31"


def generate(args, fixture):
    groups = {"ground": [], "deck": []}
    counts, timings = [], []
    owned = 0
    for row in fixture["cases"]:
        path = original(args, row)
        began = time.perf_counter()
        product, note = build_planar_rail_hardware(
            row["xy"], row["triangle"], RailRecipe(gauge_m=row["gauge_m"])
        )
        timings.append(time.perf_counter() - began)
        if product is None:
            raise ValueError(
                "library lost an originally admitted rail: " + row["id"] + str(note)
            )
        target = args.output / "pieces" / (row["id"] + ".npz")
        save_piece(target, product)
        with np.load(path) as old, np.load(target) as new:
            if set(old.files) != set(new.files) or any(
                not np.array_equal(old[k], new[k]) for k in old.files
            ):
                raise ValueError(
                    "new library changed actual admitted rail arrays: " + row["id"]
                )
        arrays = [
            product.rails.vertices,
            product.rails.triangles,
            product.sleeper_transforms,
            product.sleeper_bottoms,
            product.plane_origin,
            product.plane_frame,
            product.horizontal.rails.vertices,
            product.horizontal.rails.triangles,
            product.horizontal.sleeper_transforms,
        ]
        owned += sum(a.nbytes for a in arrays)
        groups[row["bed_kind"]].append(product)
        counts.append(
            {
                "id": row["id"],
                "sources": row["sources"],
                "sha256": digest(target),
                "counts": row["original_counts"],
                "notes": note,
            }
        )
    packs = {}
    for kind, root in [("ground", args.lower), ("deck", args.upper)]:
        data = packed_mesh(groups[kind])
        with np.load(root / "hardware.npz") as old:
            if set(old.files) != set(data) or any(
                not np.array_equal(old[k], data[k]) for k in old.files
            ):
                raise ValueError("library packed real hardware changed")
        np.savez_compressed(args.output / (kind + "-hardware.npz"), **data)
        packs[kind] = data
    return (
        packs,
        counts,
        {
            "construction_seconds": sum(timings),
            "pieces": len(counts),
            "owned_result_and_local_array_bytes": owned,
            "scope": "Construction only per case after fixed input decode; original decode, export/comparison/packing/render excluded. Result/local arrays counted, shared unit sleeper once336B separately; GEOS/internal copy totals unobserved.",
        },
    )


def scene(args, packs):
    if digest(args.latest / "after-body.npz") != LATEST_BODY:
        raise ValueError("unchanged latest full transport scene required")
    with np.load(args.latest / "after-body.npz") as d:
        body = {k: d[k] for k in d.files}
    ranges = []
    for kind, root in [("ground", args.lower), ("deck", args.upper)]:
        report = json.loads((root / "comparison.json").read_text())
        n = report["render_inputs"][0]["export"]["vertices"]
        f = report["render_inputs"][0]["export"]["triangles"]
        pack = packs[kind]
        nv, nf = len(pack["vertices"]), len(pack["triangles"])
        if not np.array_equal(
            body["vertices"][n : n + nv], pack["vertices"]
        ) or not np.array_equal(body["triangles"][f : f + nf] - n, pack["triangles"]):
            raise ValueError(
                "actual rebuilt rails are not the original latest scene ranges"
            )
        body["vertices"][n : n + nv] = pack["vertices"]
        body["triangles"][f : f + nf] = pack["triangles"] + n
        ranges.append(
            {
                "kind": kind,
                "vertex_begin": n,
                "vertices": nv,
                "face_begin": f,
                "faces": nf,
            }
        )
    path = args.output / "after-body.npz"
    np.savez_compressed(path, **body)
    with np.load(args.latest / "after-body.npz") as old:
        if any(not np.array_equal(old[k], body[k]) for k in old.files):
            raise ValueError("library migration changed latest full scene")
    camera = camera_settings(json.loads((args.latest / "after-scene.json").read_text()))
    row = dict(
        camera,
        id="Wien-planar-rail-library",
        mesh=str(path),
        mesh_sha256=digest(path),
        source_array_payload_bytes=sum(a.nbytes for a in body.values()),
        output=str(args.output / "after.png"),
    )
    (args.output / "after-scene.json").write_text(json.dumps(row, indent=2) + "\n")
    row = dict(
        row,
        profile=[1920, 1080],
        id="Wien-planar-rail-library-1080",
        output=str(args.output / "after-1080.png"),
    )
    (args.output / "after-1080-scene.json").write_text(json.dumps(row, indent=2) + "\n")
    return ranges


def build(args):
    began = time.perf_counter()
    module = Path(inspect.getfile(build_planar_rail_hardware)).resolve()
    if not module.is_relative_to(args.package_root.resolve()) or args.output.exists():
        raise ValueError(
            "fresh output and explicit actual source/wheel package root required"
        )
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
    ).strip():
        raise ValueError("clean frozen experiment source required")
    (args.output / "pieces").mkdir(parents=True)
    fixture = cases(args)
    packs, rows, work = generate(args, fixture)
    ranges = scene(args, packs)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "fixture_sha256": digest(args.inputs),
        "package_module": str(module),
        "package_module_sha256": digest(module),
        "pieces": rows,
        "construction": work,
        "latest_scene_sha256": LATEST_BODY,
        "actual_replaced_ranges": ranges,
        "scene_arrays_exact": True,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in [
                    "commit",
                    "package_module",
                    "construction",
                    "seconds",
                    "peak_rss_kib",
                ]
            }
        ),
        flush=True,
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["inputs", "lower", "upper", "latest", "package-root", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    build(arguments())
