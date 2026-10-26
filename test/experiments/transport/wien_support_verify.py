"""Replay saved physical piers, contact faces, source stations and unchanged base body."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, replay_contact
from bridge_support_plan import footprint_at, stations
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_checks import audit_closed
from wien_support_experiment import read_inputs, verify


def unpack(path):
    products, contacts = [], []
    with np.load(path) as saved:
        d = dict(saved)
    vo = fo = cv = cf = 0
    counts = zip(
        d["product_vertex_counts"],
        d["product_face_counts"],
        d["contact_vertex_counts"],
        d["contact_face_counts"],
        strict=True,
    )
    for nv, nf, nc, nt in counts:
        products.append(
            Mesh(d["vertices"][vo : vo + nv], d["triangles"][fo : fo + nf] - vo)
        )
        faces = d["contact_faces"][cf : cf + nt]
        contacts.append(
            (
                Mesh(d["contact_vertices"][cv : cv + nc], faces),
                Mesh(d["contact_vertices"][cv + nc : cv + 2 * nc], faces),
            )
        )
        vo, fo, cv, cf = vo + nv, fo + nf, cv + 2 * nc, cf + nt
    if (vo, fo, cv, cf) != tuple(
        len(d[k])
        for k in ("vertices", "triangles", "contact_vertices", "contact_faces")
    ):
        raise ValueError("support exported lengths do not match ownership")
    return products, contacts, d


def caps_match_body(body, low, high):
    n, f = len(low.vertices), len(low.triangles)
    if not (
        np.array_equal(body.vertices[:n], high.vertices)
        and np.array_equal(body.vertices[n:], low.vertices)
        and np.array_equal(body.triangles[:f], high.triangles)
        and np.array_equal(body.triangles[f : 2 * f], low.triangles[:, ::-1] + n)
    ):
        raise ValueError("support actual body differs from independently replayed caps")
    edges = low.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _keys, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
    if len(body.triangles) != 2 * f + 2 * np.count_nonzero(counts == 1):
        raise ValueError("support has unexpected interior or missing walls")
    p = low.vertices[low.triangles, :2]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    areas = (a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
    heights = high.vertices[:, 2] - low.vertices[:, 2]
    expected = float(np.sum(areas * heights[low.triangles].mean(axis=1)))
    if heights.min() < 2 or abs(audit_closed(body) - expected) > 1e-7:
        raise ValueError("support actual height or closed oriented volume invalid")
    source = body.vertices[body.triangles]
    actual = body.vertices.astype(np.float32).astype(float)[body.triangles]
    normals = np.cross(source[:, 1] - source[:, 0], source[:, 2] - source[:, 0])
    rendered = np.cross(actual[:, 1] - actual[:, 0], actual[:, 2] - actual[:, 0])
    if np.any(np.linalg.norm(rendered, axis=1) == 0) or np.any(
        np.einsum("ij,ij->i", normals, rendered) <= 0
    ):
        raise ValueError("support float32 presentation degenerates or inverts faces")


def replay_method(root, name, report, sources, lower, upper, excluded, world):
    path = root / (name + ".npz")
    if digest(path) != report["mesh_sha256"]:
        raise ValueError("support export changed")
    products, contacts, _data = unpack(path)
    candidates = stations([r for r in sources if r["role"] == "deck"])
    if len(report["stations"]) != len(candidates):
        raise ValueError("support station ledger incomplete")
    shapes, errors, used = [], [], []
    for candidate, row in zip(candidates, report["stations"], strict=True):
        if (candidate["source_id"], candidate["station_m"]) != (
            row["source_id"],
            row["station_m"],
        ):
            raise ValueError("support source station ownership changed")
        if not row["valid"]:
            if not row["reason"]:
                raise ValueError("support omission reason absent")
            continue
        if row["product"] != len(used) or row["shift_m"] not in (
            (0,) if name == "fixed" else (0, -3, 3, -6, 6)
        ):
            raise ValueError("support product ownership or shift invalid")
        used.append(row)
        lo, hi = contacts[row["product"]]
        caps_match_body(products[row["product"]], lo, hi)
        expected = footprint_at(
            candidate["line"], candidate["station_m"] + row["shift_m"]
        )
        shape = surface_domain(lo)
        if (
            shape.symmetric_difference(expected).area > 1e-8
            or shape.hausdorff_distance(expected) > 1e-7
        ):
            raise ValueError("support actual footprint differs from declared station")
        errors.extend((replay_contact(lo, lower), replay_contact(hi, upper)))
        shapes.append(shape)
    if len(used) != len(products) or len(products) != report["accepted"]:
        raise ValueError("support product ledger incomplete")
    return dict(
        verify(products, contacts, shapes, lower, upper, excluded, world),
        maximum_contact_error_m=max(errors, default=0),
    )


def replay(root, network, reference):
    report = json.loads((root / "comparison.json").read_text())
    if (
        report["dirty"]
        or digest(network / "comparison.json") != report["network_report_sha256"]
    ):
        raise ValueError("clean support/base binding required")
    sources, ground, deck, excluded, world, base, _fixture = read_inputs(
        network, reference
    )
    with np.load(network / "ground.npz") as saved:
        traffic = Mesh(ground.vertices, ground.triangles[saved["road"]])
    declared = shapely.from_wkb((network / "traffic.wkb").read_bytes())
    if (
        surface_domain(traffic).symmetric_difference(declared).area > 1e-7
        or surface_domain(ground).symmetric_difference(world).area > 1e-7
    ):
        raise ValueError("support exclusions differ from actual pinned ground/traffic")
    lower, upper = ContactSurface(ground), ContactSurface(deck)
    notes = {
        name: replay_method(root, name, row, sources, lower, upper, excluded, world)
        for name, row in report["methods"].items()
    }
    _products, _contacts, pier = unpack(root / (report["selected"] + ".npz"))
    path = root / "after-body.npz"
    if digest(path) != report["render_inputs"][1]["export"]["sha256"]:
        raise ValueError("rendered support body changed")
    with np.load(path) as data:
        n, f = len(base[0].vertices), len(base[0].triangles)
        for a, b in (
            (data["vertices"][:n], base[0].vertices),
            (data["triangles"][:f], base[0].triangles),
            (data["materials"][:f], base[1]),
            (data["vertices"][n:], pier["vertices"]),
            (data["triangles"][f:] - n, pier["triangles"]),
        ):
            if not np.array_equal(a, b):
                raise ValueError(
                    "presentation changes original geometry or actual piers"
                )
    return {
        "comparison_sha256": digest(root / "comparison.json"),
        "methods": notes,
        "unchanged_base_geometry_and_materials": True,
        "selected": report["selected"],
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh independent replay report required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    result = replay(args.products, args.network, args.reference_root)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
