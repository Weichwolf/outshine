"""Bounded physical outside-road profiles using the existing whole-contact volume kernel."""

import argparse
import json
import resource
import subprocess
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from camera_height_inputs import digest
from curb_geometry import audit
from curb_inputs import inputs
from curb_plan import HEIGHT_M, WIDTH_M
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportBudget, SupportVolumeBuilder
from transport_surface_experiment import PALETTE, camera_settings
from wien_support_experiment import packed

HERE = Path(__file__).resolve().parent
BUDGET = SupportBudget(maximum_footprint_coordinates=1024)


def builder(mesh):
    return SupportVolumeBuilder(
        mesh, Mesh(mesh.vertices + [0, 0, HEIGHT_M], mesh.triangles), BUDGET
    )


def attempt(candidate, source, context):
    product, note = source.build(candidate["shape"], minimum_height_m=0.1199999)
    if product is None:
        return None, dict(note)
    try:
        proof = audit(
            product, candidate["shape"], *context[:4], outside_limit=context[4]
        )
    except ValueError as error:
        return None, dict(note, valid=False, reason=str(error))
    return product, dict(note, audit=proof)


def save_products(path, products):
    data = packed(
        [p.body for p in products],
        [(p.lower_contact, p.upper_contact) for p in products],
    )
    np.savez_compressed(path, **data)
    return data


def compare(candidates, ground, field, context, output):
    indices = np.unique(
        np.linspace(0, len(candidates) - 1, min(12, len(candidates))).astype(int)
    )
    reports = {}
    for name, mesh in [("fine", ground), ("field", field)]:
        source = builder(mesh)
        rows = []
        for index in indices:
            candidate = candidates[index]
            if candidate["reason"]:
                rows.append(
                    {
                        "candidate": int(index),
                        "valid": False,
                        "reason": candidate["reason"],
                    }
                )
                continue
            product, note = attempt(candidate, source, context)
            row = dict(note, candidate=int(index))
            if product is not None:
                path = output / f"comparison-{name}-{index}.npz"
                save_products(path, [product])
                row.update(path=str(path), sha256=digest(path))
            rows.append(row)
        reports[name] = {
            "preparation_seconds": source.preparation_seconds,
            "rows": rows,
        }
    return reports


def generate(candidates, field, context, output):
    source = builder(field)
    products = []
    rows = []
    began = time.perf_counter()
    faces = 0
    for candidate in candidates:
        row = {k: v for k, v in candidate.items() if k != "shape"}
        row.update(
            valid=False, footprint_wkb_hex=shapely.to_wkb(candidate["shape"], hex=True)
        )
        if row["reason"]:
            pass
        elif time.perf_counter() - began > 45 or len(products) >= 256:
            row["reason"] = "curb_optional_piece_or_time_budget"
        else:
            product, note = attempt(candidate, source, context)
            row.update(note)
            if product is not None:
                if faces + len(product.body.triangles) > 262144:
                    row.update(valid=False, reason="curb_whole_face_budget")
                else:
                    row["product"] = len(products)
                    products.append(product)
                    faces += len(product.body.triangles)
        rows.append(row)
    if not products:
        raise ValueError(
            "no admissible optional curb body; old baseline remains available"
        )
    data = save_products(output / "curbs.npz", products)
    return (
        data,
        rows,
        {
            "preparation_seconds": source.preparation_seconds,
            "generation_and_audit_seconds": time.perf_counter() - began,
            "accepted": len(products),
            "reasons": dict(Counter(r["reason"] for r in rows if not r["valid"])),
        },
    )


def scenes(output, baseline, before, curbs):
    camera = camera_settings(json.loads((baseline / "after-scene.json").read_text()))
    catalogue = {r["name"]: r for r in json.loads(PALETTE.read_text())["classes"]}
    colour = np.array([*catalogue["concrete"]["albedo"], 1], np.float32)
    camera.update(colours=camera["colours"] + [colour.tolist()], span_m=1100)
    p = curbs["vertices"][curbs["triangles"]]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normals = (normals / np.linalg.norm(normals, axis=1)[:, None]).astype(np.float32)
    after = {
        "vertices": np.vstack([before["vertices"], curbs["vertices"]]),
        "triangles": np.vstack(
            [before["triangles"], curbs["triangles"] + len(before["vertices"])]
        ),
        "normals": np.vstack([before["normals"], np.repeat(normals, 3, axis=0)]),
        "materials": np.r_[
            before["materials"],
            np.full(len(curbs["triangles"]), 10, dtype=before["materials"].dtype),
        ],
        "face_base_colour_linear_rgba": np.vstack(
            [
                before["face_base_colour_linear_rgba"],
                np.tile(colour, (len(curbs["triangles"]), 1)),
            ]
        ),
    }
    np.savez_compressed(output / "after-body.npz", **after)
    records = []
    for name, path, arrays in [
        ("before", baseline / "after-body.npz", before),
        ("after", output / "after-body.npz", after),
    ]:
        row = dict(
            camera,
            id="Wien-curbs-" + name,
            mesh=str(path),
            mesh_sha256=digest(path),
            source_array_payload_bytes=sum(a.nbytes for a in arrays.values()),
            output=str(output / (name + ".png")),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        records.append(row)
    high = dict(
        records[1],
        profile=[1920, 1080],
        id="Wien-curbs-after-1080",
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(high, indent=2) + "\n")
    return records, {
        "material_json_sha256": digest(PALETTE),
        "procedural_material": "concrete",
        "linear_rgba": colour.tolist(),
        "new_indexed_xyz_copy_bytes": p.nbytes,
        "new_corner_normal_copy_bytes": len(curbs["triangles"]) * 3 * 3 * 4,
    }


def build(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and clean frozen curb source required")
    args.output.mkdir(parents=True)
    ground, field, before, candidates, traffic, world, excluded, binding, work = inputs(
        args.network, args.baseline, args.supports, args.reference_root
    )
    reference = ContactSurface(ground)
    context = (
        reference,
        traffic,
        world,
        excluded,
        traffic.buffer(WIDTH_M, quad_segs=4),
    )
    comparisons = compare(candidates, ground, field, context, args.output)
    curbs, rows, generation = generate(candidates, field, context, args.output)
    records, presentation = scenes(args.output, args.baseline, before, curbs)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": binding,
        "selection": work,
        "comparison": comparisons,
        "candidates": rows,
        "generation": generation,
        "render_inputs": records,
        "presentation": presentation,
        "curbs_sha256": digest(args.output / "curbs.npz"),
        "area_m2": sum(r["audit"]["area_m2"] for r in rows if r["valid"]),
        "vertices": len(curbs["vertices"]),
        "faces": len(curbs["triangles"]),
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Procedural exterior curb profiles only; existing traffic, all old bodies/material fields and source use unchanged. Explicit crossing/port/upper-structure gaps and local bounded omissions. No continuous sidewalk, barrier-free route, building reservation, native or device claim.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "generation": generation,
                "area_m2": report["area_m2"],
                "faces": report["faces"],
                "seconds": report["seconds"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["network", "baseline", "supports", "reference-root", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    build(args)
