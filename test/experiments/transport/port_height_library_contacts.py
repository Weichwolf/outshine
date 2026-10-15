"""Actual finite contact replay at every original road vertex and whole-port centre."""

import argparse
import hashlib
import json
import time
from collections import Counter
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from port_plane_fixture import load_reference


def replay(directory):
    report = json.loads((directory / "report.json").read_text())
    path = directory / report["mesh_file"]
    if (
        not report["valid_geometry"]
        or hashlib.sha256(path.read_bytes()).hexdigest() != report["mesh_sha256"]
    ):
        raise ValueError("unchanged accepted finite plan required")
    product, captured, _metadata, _sources = load_reference()
    with np.load(path) as data:
        mesh = Mesh(np.c_[data["xy"], data["heights"]], data["faces"])
        original = data["original_vertex_map"]
    road = original[captured["road_vertex_map"]]
    port_edges = road[product["port_edges"]]
    points = np.vstack([mesh.vertices[road], mesh.vertices[port_edges].mean(axis=1)])
    results = []
    scale = max(1, abs(mesh.vertices[:, :2]).max())
    for tolerance in (1e-8, 64 * np.finfo(float).eps * scale, 0.0):
        began = time.monotonic()
        sampler = SurfaceSampler(mesh, xy_tolerance_m=tolerance, max_faces=262144)
        prepared = time.monotonic()
        sampled = sampler.sample(
            points[:, :2], max_queries=65536, max_candidates=1000000
        )
        valid = sampled.face_indices >= 0
        delta = abs(sampled.points_enu_m[valid, 2] - points[valid, 2])
        if delta.max(initial=0) > 1e-7:
            raise ValueError(
                "finite contact height exceeds the existing metric tolerance"
            )
        result = {
            "xy_tolerance_m": tolerance,
            "queries": len(points),
            "original_road_vertices": len(road),
            "original_ports": len(port_edges),
            "valid": int(valid.sum()),
            "maximum_height_error_m": float(delta.max(initial=0)),
            "reasons": dict(Counter(reason for reason in sampled.reasons if reason)),
            "unresolved": [
                {
                    "query": int(i),
                    "kind": "original_road_vertex"
                    if i < len(road)
                    else "original_port_centre",
                    "input_id": int(i) if i < len(road) else int(i - len(road)),
                    "xy_m": points[i, :2].tolist(),
                    "reason": sampled.reasons[i],
                }
                for i in np.flatnonzero(~valid)
            ],
            "preparation_ms": (prepared - began) * 1000,
            "query_report": dict(sampled.report),
            "total_ms": (time.monotonic() - began) * 1000,
        }
        results.append(result)
        print(json.dumps(result), flush=True)
    output = {
        "generation_commit": report["generation_commit"],
        "mesh_sha256": report["mesh_sha256"],
        "frame": report["frame"],
        "cases": results,
        "scope": "Caller-selected actual finite road/soil mesh. Analytic patch continuation and automatic highest-surface selection are not used. Missing contacts keep explicit reasons.",
    }
    (directory / "finite-contacts.json").write_text(json.dumps(output, indent=2) + "\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    replay(parser.parse_args().directory)
