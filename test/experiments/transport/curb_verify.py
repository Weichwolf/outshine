"""Independently read actual saved curb bodies/caps and verify original scene prefix."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from camera_height_inputs import digest
from curb_geometry import audit
from curb_inputs import inputs
from curb_plan import WIDTH_M
from outshine_ref.transport.supports import SupportVolume
from retained_surface import surface_domain
from wien_support_verify import unpack


def actual(path):
    bodies, contacts, data = unpack(path)
    return [SupportVolume(b, *c) for b, c in zip(bodies, contacts, strict=True)], data


def verify_display(args, report, before, curbs):
    row = report["render_inputs"][1]
    path = Path(row["mesh"])
    if digest(path) != row["mesh_sha256"]:
        raise ValueError("curb rendered export changed")
    with np.load(path) as d:
        if (
            set(d.files) != set(before)
            or sum(d[k].nbytes for k in d.files) != row["source_array_payload_bytes"]
        ):
            raise ValueError("curb scene array ownership/payload mismatch")
        for key, value in before.items():
            if not np.array_equal(d[key][: len(value)], value):
                raise ValueError(
                    "old terrain/traffic/hardware/material/normal prefix changed"
                )
        n, f = len(before["vertices"]), len(before["triangles"])
        if not (
            np.array_equal(d["vertices"][n:], curbs["vertices"])
            and np.array_equal(d["triangles"][f:] - n, curbs["triangles"])
            and np.all(d["materials"][f:] == 10)
        ):
            raise ValueError(
                "actual rendered curbs differ from independently read bodies"
            )
        colour = np.asarray(report["presentation"]["linear_rgba"], np.float32)
        if not np.array_equal(
            d["face_base_colour_linear_rgba"][f:],
            np.tile(colour, (len(curbs["triangles"]), 1)),
        ):
            raise ValueError("actual curb source albedo differs from declared material")
        p = curbs["vertices"][curbs["triangles"]]
        normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        normals = (normal / np.linalg.norm(normal, axis=1)[:, None]).astype(np.float32)
        if not np.array_equal(d["normals"][3 * f :], np.repeat(normals, 3, axis=0)):
            raise ValueError("actual provided curb normals differ from source faces")
    return {
        "old_arrays_prefix_exact": True,
        "new_vertices": len(curbs["vertices"]),
        "new_faces": len(curbs["triangles"]),
        "normals_and_albedo_exact": True,
    }


def replay(args):
    report = json.loads((args.products / "comparison.json").read_text())
    ground, _field, before, candidates, traffic, world, excluded, binding, _work = (
        inputs(args.network, args.baseline, args.supports, args.reference_root)
    )
    if (
        report["dirty"]
        or report["bindings"] != binding
        or len(report["candidates"]) != len(candidates)
    ):
        raise ValueError("clean bound complete curb source ledger required")
    context = (ContactSurface(ground), traffic, world, excluded)
    limit = traffic.buffer(WIDTH_M, quad_segs=4)
    for value in report["comparison"].values():
        for row in value["rows"]:
            if row["valid"]:
                path = Path(row["path"])
                if digest(path) != row["sha256"]:
                    raise ValueError("curb comparison export changed")
                bodies, _ = actual(path)
                if len(bodies) != 1:
                    raise ValueError("one actual body per comparison case required")
                audit(
                    bodies[0],
                    candidates[row["candidate"]]["shape"],
                    *context,
                    outside_limit=limit,
                )
            elif not row["reason"]:
                raise ValueError("curb comparison omission reason missing")
    path = args.products / "curbs.npz"
    if digest(path) != report["curbs_sha256"]:
        raise ValueError("saved actual curb bodies changed")
    products, data = actual(path)
    proofs = []
    shapes = []
    for candidate, row in zip(candidates, report["candidates"], strict=True):
        shape = shapely.from_wkb(bytes.fromhex(row["footprint_wkb_hex"]))
        if (
            row["sources"] != candidate["sources"]
            or shape.symmetric_difference(candidate["shape"]).area > 1e-8
        ):
            raise ValueError("curb source/finite footprint selection changed")
        if not row["valid"]:
            if not row["reason"]:
                raise ValueError("local curb omission reason missing")
            continue
        if row["product"] != len(proofs):
            raise ValueError("curb body ownership changed")
        product = products[row["product"]]
        proofs.append(audit(product, shape, *context, outside_limit=limit))
        shapes.append(surface_domain(product.lower_contact))
    if len(proofs) != len(products):
        raise ValueError("actual curb body ledger incomplete")
    tree = shapely.STRtree(shapes)
    pairs = tree.query(shapes, predicate="intersects")
    chosen = pairs[0] < pairs[1]
    for a, b in pairs[:, chosen].T:
        if shapes[a].intersection(shapes[b]).area > 1e-8:
            raise ValueError("actual curb bodies overlap")
    return {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "bodies": len(proofs),
        "area_m2": sum(p["area_m2"] for p in proofs),
        "maximum_contact_error_m": max(
            p["whole_ground_contact_error_m"] for p in proofs
        ),
        "maximum_height_error_m": max(p["height_error_m"] for p in proofs),
        "maximum_volume_error_m3": max(p["volume_error_m3"] for p in proofs),
        "mutual_pair_candidates": pairs.shape[1],
        "display": verify_display(args, report, before, data),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in [
        "products",
        "network",
        "baseline",
        "supports",
        "reference-root",
        "output",
    ]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh independent curb proof required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    result = replay(args)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
