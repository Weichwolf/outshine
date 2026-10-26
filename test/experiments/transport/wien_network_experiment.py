"""Rebuild Vienna ground traffic around unchanged upper bridge/portal research."""

import argparse
import json
import os
import resource
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from affine_field_surface import mesh_field
from camera_height_inputs import digest, lumped_weights
from capture_height_targets import sample_targets
from ground_surface_showcase import body, combined
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_checks import audit_closed
from transport_field_plan import dense_input, field_input, solve_input
from tunnel_portal_showcase import assembled
from whole_tile_showcase import COLOURS
from wien_ground_inputs import tile_domain
from wien_network_sources import full_ledger, scene, select
from wien_network_union import union_traffic
from wien_network_verify import verify

HERE = Path(__file__).resolve().parent
BASIS_PIN = "e740d969b6b6baa230c50cb1c3475350d8610b1685119c2bb619732acde89442"


def load_flat(root):
    directory = root / "results/ViennaNetwork/components"
    report = json.loads((directory / "result.json").read_text())
    if report["input_sha256"] != digest(root / "inputs/ViennaNetwork-input.json"):
        raise ValueError("component source binding changed")
    positions, faces, offset = [], [], 0
    products = []
    for index in range(report["products"]):
        path = directory / f"product-{index}.npz"
        with np.load(path) as data:
            xy, indices = data["vertices"], data["indices"]
        positions.append(xy)
        faces.append(indices + offset)
        offset += len(xy)
        products.append({"path": str(path), "sha256": digest(path)})
    if not positions:
        raise ValueError("new component plan absent; old products remain untouched")
    mesh = Mesh(np.c_[np.vstack(positions), np.zeros(offset)], np.vstack(faces))
    return mesh, {"report": report, "products": products}


def generate_flat(output):
    subprocess.run(
        [
            sys.executable,
            "-c",
            "import subprocess,sys;subprocess.run(sys.argv[1:],check=True,timeout=110)",
            sys.executable,
            str(HERE / "component_surface_experiment.py"),
            "--data-root",
            str(output),
            "--worker",
            "ViennaNetwork",
            "components",
        ],
        check=True,
        timeout=120,
    )
    return load_flat(output)


def fit_case(name, arrays, meta, output):
    source = output / (name + "-input.npz")
    np.savez_compressed(source, **arrays)
    began = time.perf_counter()
    mesh, report = solve_input(arrays)
    meta.update(
        input_sha256=digest(source), fit=report, seconds=time.perf_counter() - began
    )
    if mesh is not None:
        path = output / (name + ".npz")
        np.savez_compressed(path, vertices=mesh.vertices, faces=mesh.triangles)
        meta["mesh_sha256"] = digest(path)
    (output / (name + ".json")).write_text(json.dumps(meta, indent=2) + "\n")
    print(
        json.dumps(
            {
                "phase": name,
                "valid": report["valid"],
                "vertices": len(arrays["xy"]),
                "faces": len(arrays["faces"]),
                "seconds": meta["seconds"],
            }
        ),
        flush=True,
    )
    return mesh, meta


def compare_assembly(flat, domain, previous, roof, prior, output):
    reports, united = {}, None
    for method in ("cells", "domain"):
        try:
            candidate, report = union_traffic(previous[1], flat, method)
            path = output / (method + "-union.npz")
            np.savez_compressed(
                path, vertices=candidate.vertices, faces=candidate.triangles
            )
            reports[method] = dict(report, valid=True, mesh_sha256=digest(path))
            united = candidate
        except (ValueError, shapely.errors.GEOSException) as failure:
            reports[method] = {"valid": False, "reason": str(failure)}
    try:
        if united is None:
            raise ValueError("no valid dense traffic layout")
        arrays, meta = dense_input(
            united, domain, previous[0], roof, previous[1], prior
        )
        _dense, dense_note = fit_case("dense", arrays, meta, output)
    except (ValueError, shapely.errors.GEOSException) as failure:
        dense_note = {
            "fit": {"valid": False, "reason": str(failure)},
            "phase": "dense_preparation",
        }
        (output / "dense.json").write_text(json.dumps(dense_note, indent=2) + "\n")
    return reports, dense_note


def export_body(mesh, slots, path):
    xyz = mesh.vertices[mesh.triangles]
    normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    if not np.all(np.isfinite(lengths)) or np.any(lengths == 0):
        raise ValueError("closed source body has a degenerate normal")
    normals = np.repeat((normals / lengths[:, None]).astype(np.float32), 3, axis=0)
    np.savez_compressed(
        path,
        vertices=mesh.vertices,
        triangles=mesh.triangles,
        normals=normals,
        materials=slots,
    )
    return {
        "path": str(path),
        "sha256": digest(path),
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.triangles),
        "supplied_corner_normals": len(normals),
        "closed_volume_m3": audit_closed(mesh),
        "exported_array_bytes": mesh.vertices.nbytes
        + mesh.triangles.nbytes
        + normals.nbytes
        + slots.nbytes,
        "explicit_indexed_xyz_copy_bytes": xyz.nbytes,
        "explicit_corner_normal_copy_bytes": normals.nbytes,
        "copy_count_scope": "These two explicit NumPy copies only; not all pipeline copies.",
    }


def render_inputs(args, after, slots, baseline):
    with np.load(args.basis_reference / "solid.npz") as data:
        old = Mesh(data["vertices"], data["triangles"])
        old_slots = data["materials"]
    camera = {
        k: baseline["after"]["scene"][k]
        for k in ("eye", "target", "span_m", "detail_view")
    }
    rows = []
    for label, geometry, materials in (
        ("before", old, old_slots),
        ("after", after, slots),
    ):
        row = export_body(geometry, materials, args.output / (label + "-body.npz"))
        row.update(
            id="Wien-" + label,
            mesh=row["path"],
            mesh_sha256=row["sha256"],
            projection="orthographic",
            profile=[1280, 720],
            samples=16,
            colours=COLOURS,
            output=str(args.output / (label + ".png")),
            **camera,
        )
        (args.output / (label + "-scene.json")).write_text(
            json.dumps(row, indent=2) + "\n"
        )
        rows.append(row)
    detail = dict(
        rows[1],
        id="Wien-after-1080",
        profile=[1920, 1080],
        output=str(args.output / "after-1080.png"),
    )
    (args.output / "after-1080-scene.json").write_text(
        json.dumps(detail, indent=2) + "\n"
    )
    return rows


def baseline_input(args):
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError("fresh empty network output required")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
    ).strip():
        raise ValueError("network comparison requires a frozen clean source")
    if digest(args.basis_reference / "input.npz") != BASIS_PIN:
        raise ValueError("pinned old ground support changed")
    baseline = json.loads((args.basis_reference / "comparison.json").read_text())
    if (
        baseline["dirty"]
        or digest(args.basis_reference / "solid.npz") != baseline["solid_sha256"]
    ):
        raise ValueError("clean baseline body required")
    return baseline


def build(args):
    began = time.perf_counter()
    baseline = baseline_input(args)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "inputs").mkdir()
    fixture, previous, model, note = scene(args.reference_root)
    source, old_ledger = select(args.capture, fixture, note, model, previous)
    selected_path = args.output / "inputs/ViennaNetwork-input.json"
    selected_path.write_text(json.dumps(source) + "\n")
    flat, flat_note = generate_flat(args.output)
    planned = json.loads(
        (args.output / "results/ViennaNetwork/components/source-usage.json").read_text()
    )
    ledger = full_ledger(source, old_ledger, planned)
    traffic = surface_domain(previous[1]).union(surface_domain(flat))
    domain = (
        tile_domain(fixture, previous[1])
        .union(traffic.buffer(4))
        .difference(model["open_trench"])
    )
    if domain.intersection(traffic).symmetric_difference(traffic).area > 1e-7:
        raise ValueError("new traffic intrudes into a retained open portal")
    for label, shape in (("traffic", traffic), ("world", domain)):
        (args.output / (label + ".wkb")).write_bytes(shape.wkb)
    prior = lambda xy: sample_targets(
        fixture["origin_lat_lon"], xy, args.dem_sources, 11, fetch=False
    )
    union_notes, dense_note = compare_assembly(
        flat, domain, previous, model["roof"], prior, args.output
    )
    with np.load(args.basis_reference / "input.npz") as data:
        seed = Mesh(np.c_[data["xy"], np.zeros(len(data["xy"]))], data["faces"])
    masks = np.r_[
        shapely.polygons(flat.vertices[flat.triangles, :2]),
        shapely.polygons(previous[1].vertices[previous[1].triangles, :2]),
    ]
    arrays, meta = field_input(
        seed, masks, traffic, domain, previous[0], model["roof"], previous[1], prior
    )
    field, field_note = fit_case("field", arrays, meta, args.output)
    if field is None:
        raise ValueError("shared height field absent; old products remain untouched")
    mesh, road, projection = mesh_field(field, traffic, domain)
    print(json.dumps(dict(phase="actual_mesh", **projection)), flush=True)
    np.savez_compressed(
        args.output / "ground.npz",
        vertices=mesh.vertices,
        faces=mesh.triangles,
        road=road,
    )
    proof = verify(
        mesh, road, traffic, domain, fixture, previous, model, note, source, ledger
    )
    structural, structural_slots = assembled(
        {k: v for k, v in model.items() if k != "ground"}, previous
    )
    solid, slots = combined(
        [(structural, structural_slots), body(mesh, np.where(road, 0, 5), 0.25)]
    )
    rows = render_inputs(args, solid, slots, baseline)
    target, dem = prior(mesh.vertices[:, :2])
    weights = lumped_weights(mesh.vertices[:, :2], mesh.triangles)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "capture_sha256": digest(args.capture),
        "selected_input_sha256": digest(selected_path),
        "flat": flat_note,
        "union_comparison": union_notes,
        "dense": dense_note,
        "field": field_note,
        "projection": projection,
        "verification": proof,
        "source_usage": ledger,
        "source_counts": dict(Counter(r["status"] for r in ledger)),
        "render_inputs": rows,
        "dem_receipts": dem,
        "area_weighted_dem_vertex_rms_m": float(
            np.sqrt(np.average((mesh.vertices[:, 2] - target) ** 2, weights=weights))
        ),
        "ground_sha256": digest(args.output / "ground.npz"),
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Python reference partial network; original upper C1/portal products retained. Changed lower contacts, implicit source binding and full routing/native integration remain separate. All images local.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(args.output),
                "seconds": result["seconds"],
                "sources": result["source_counts"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "reference-root",
        "capture",
        "basis-reference",
        "dem-sources",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    arguments = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    os.environ["OPENBLAS_NUM_THREADS"] = os.environ["OMP_NUM_THREADS"] = "1"
    build(arguments)
