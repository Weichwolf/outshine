"""Offline bounded physical piers on the unchanged verified Vienna network."""

import argparse
import json
import resource
import subprocess
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, replay_contact
from bridge_support_plan import plan_supports, stations
from camera_height_inputs import digest
from ground_surface_showcase import combined
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from retained_surface import surface_domain
from wien_network_experiment import export_body
from wien_network_sources import scene

HERE = Path(__file__).resolve().parent
BRIDGE_PIN = "fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4"
GROUND_PIN = "16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5"


def read_inputs(root, reference):
    report = json.loads((root / "comparison.json").read_text())
    if report["dirty"] or digest(root / "ground.npz") != GROUND_PIN:
        raise ValueError("clean pinned complete network ground required")
    if digest(root / "after-body.npz") != report["render_inputs"][1]["mesh_sha256"]:
        raise ValueError("pinned unchanged complete network body required")
    source = HERE / "fixtures/bridge-continuations-vienna.json"
    if digest(source) != BRIDGE_PIN:
        raise ValueError("pinned original bridge axes required")
    sources = json.loads(source.read_text())["sources"]
    fixture, previous, model, _note = scene(reference)
    with np.load(root / "ground.npz") as data:
        ground = Mesh(data["vertices"], data["faces"])
    with np.load(root / "after-body.npz") as data:
        before = Mesh(data["vertices"], data["triangles"]), data["materials"]
    traffic = shapely.from_wkb((root / "traffic.wkb").read_bytes())
    world = shapely.from_wkb((root / "world.wkb").read_bytes())
    forbidden = shapely.union_all(
        [traffic, model["open_trench"], surface_domain(model["roof"])]
    )
    excluded = forbidden.buffer(0.5)
    upper = Mesh(previous[0].vertices - [0, 0, 0.35], previous[0].triangles)
    return sources, ground, upper, excluded, world, before, fixture


def verify(products, contacts, shapes, lower, upper, excluded, world):
    errors = []
    for pair, shape in zip(contacts, shapes, strict=True):
        if shape.intersects(excluded) or not world.covers(shape):
            raise ValueError("actual support violates traffic/portal/world domain")
        errors.extend((replay_contact(pair[0], lower), replay_contact(pair[1], upper)))
    tree = shapely.STRtree(shapes)
    pairs = tree.query(shapes, predicate="intersects")
    if np.any(pairs[0] != pairs[1]):
        raise ValueError("actual piers intersect each other")
    return {
        "piers": len(products),
        "whole_contact_error_m": max(errors, default=0),
        "pier_pair_candidates": pairs.shape[1],
        "traffic_portal_intersections": 0,
        "outside_world": 0,
    }


def write_scenes(root, output, before, products):
    pier_parts = [(mesh, np.full(len(mesh.triangles), 2, int)) for mesh in products]
    after = combined([before, *pier_parts])
    camera = json.loads((root / "after-scene.json").read_text())
    rows = []
    for name, (mesh, slots) in (("before", before), ("after", after)):
        proof = export_body(mesh, slots, output / (name + "-body.npz"))
        row = dict(
            camera,
            **proof,
            id="Wien-piers-" + name,
            mesh=proof["path"],
            mesh_sha256=proof["sha256"],
            span_m=1100,
            output=str(output / (name + ".png")),
        )
        (output / (name + "-scene.json")).write_text(json.dumps(row, indent=2) + "\n")
        rows.append({"export": proof, "scene": row})
    detail = dict(
        rows[1]["scene"],
        id="Wien-piers-after-1080",
        profile=[1920, 1080],
        output=str(output / "after-1080.png"),
    )
    (output / "after-1080-scene.json").write_text(json.dumps(detail, indent=2) + "\n")
    return rows


def build(args):
    began = time.perf_counter()
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError("fresh support output required")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
    ).strip():
        raise ValueError("clean frozen support generation required")
    args.output.mkdir(parents=True, exist_ok=True)
    sources, ground, deck, excluded, world, before, fixture = read_inputs(
        args.network, args.reference_root
    )
    candidates = stations([r for r in sources if r["role"] == "deck"])
    builder = SupportVolumeBuilder(ground, deck)
    preparation_seconds = builder.preparation_seconds
    methods, cases, selected = {}, {}, None
    for method in ("fixed", "shifted"):
        products, contacts, shapes, report = plan_supports(
            candidates, builder, excluded, method=method
        )
        cases[method] = products, contacts, shapes
        report["reasons"] = dict(
            Counter(r["reason"] for r in report["stations"] if not r["valid"])
        )
        report["accepted"] = len(products)
        np.savez_compressed(
            args.output / (method + ".npz"), **packed(products, contacts)
        )
        report["mesh_sha256"] = digest(args.output / (method + ".npz"))
        methods[method] = report
        if selected is None or len(products) > len(selected):
            selected, chosen = products, method
        print(
            json.dumps(
                {
                    "phase": method,
                    "accepted": len(products),
                    "seconds": report["seconds"],
                    "reasons": report["reasons"],
                }
            ),
            flush=True,
        )
    if not selected:
        raise ValueError("no valid piers; unchanged network remains valid")
    del builder
    lower, upper = ContactSurface(ground), ContactSurface(deck)
    for method, (products, contacts, shapes) in cases.items():
        methods[method]["verification"] = verify(
            products, contacts, shapes, lower, upper, excluded, world
        )
    del lower, upper
    rows = write_scenes(args.network, args.output, before, selected)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "network_report_sha256": digest(args.network / "comparison.json"),
        "ground_sha256": GROUND_PIN,
        "bridge_sources_sha256": BRIDGE_PIN,
        "origin_lat_lon": fixture["origin_lat_lon"],
        "methods": methods,
        "selected": chosen,
        "support_preparation_seconds": preparation_seconds,
        "render_inputs": rows,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Procedural geometric supports only. Exact selected deck-underside/ground contacts, conservative traffic/portal gaps; no load capacity, foundations, abutments, maximum safe span, complete network or native claim. Original source usage unchanged.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: result[k] for k in ("commit", "selected", "seconds", "peak_rss_kib")}
        ),
        flush=True,
    )


def packed(products, contacts):
    mesh, _slots = (
        combined([(p, np.zeros(len(p.triangles), int)) for p in products])
        if products
        else (Mesh(np.empty((0, 3)), np.empty((0, 3), int)), [])
    )
    return {
        "vertices": mesh.vertices,
        "triangles": mesh.triangles,
        "product_vertex_counts": np.array([len(p.vertices) for p in products]),
        "product_face_counts": np.array([len(p.triangles) for p in products]),
        "contact_vertices": np.concatenate(
            [np.vstack((a.vertices, b.vertices)) for a, b in contacts]
        )
        if contacts
        else np.empty((0, 3)),
        "contact_vertex_counts": np.array([len(a.vertices) for a, _b in contacts]),
        "contact_faces": np.concatenate([a.triangles for a, _b in contacts])
        if contacts
        else np.empty((0, 3), int),
        "contact_face_counts": np.array([len(a.triangles) for a, _b in contacts]),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    build(args)
