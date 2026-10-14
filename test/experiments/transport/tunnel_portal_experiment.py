"""Measured complete portal products and source usage on pinned real inputs."""

import argparse
import hashlib
import json
import os
import resource
import subprocess
import sys
import time

import numpy as np
import shapely
from capture_bridge_continuations import FIXTURE as BRIDGE_FIXTURE
from capture_tunnel_portals import PIN
from outshine_ref.contracts import HeightInterval, LocalFrame, Reservation
from outshine_ref.transport import Plan
from tunnel_portal_recipe import HEADROOM_M, ROOF_M, build
from tunnel_portal_showcase import OUTPUT, assembled, inputs


def complete(fixture, previous, method):
    model, report = build(fixture, previous, method=method)
    if not report["valid"]:
        raise ValueError(report["reason"])
    solid, materials = assembled(model, previous)
    reservations, cells = [], []
    for source in fixture["sources"]:
        footprint = shapely.LineString(source["xy"]).buffer(
            source["width_m"] / 2, cap_style="flat", join_style="mitre", mitre_limit=2
        )
        floor = model["floor"]
        shapes = shapely.polygons(floor.vertices[floor.triangles, :2])
        for index in shapely.STRtree(shapes).query(footprint, predicate="intersects"):
            region = shapes[index].intersection(footprint)
            if region.area <= 0:
                continue
            xyz = floor.vertices[floor.triangles[index]]
            lo, hi = float(xyz[:, 2].min()), float(xyz[:, 2].max())
            for purpose, low, high in (
                ("traffic_clearance", lo, hi + HEADROOM_M),
                ("transport_structure", lo - 0.35, hi),
            ):
                reservations.append(
                    Reservation(
                        source["source_id"], region, HeightInterval(low, high), purpose
                    )
                )
                cells.append(
                    {
                        "source_id": source["source_id"],
                        "purpose": purpose,
                        "xy": shapely.to_geojson(region),
                        "bounds_m": [low, high],
                    }
                )
    for source in fixture["sources"]:
        footprint = shapely.LineString(source["xy"]).buffer(
            source["width_m"] / 2 + 0.35,
            cap_style="flat",
            join_style="mitre",
            mitre_limit=2,
        )
        for name, bottom, top in (("walls", 0.0, HEADROOM_M), ("roof", -ROOF_M, 0.0)):
            mesh = model[name]
            shapes = shapely.polygons(mesh.vertices[mesh.triangles, :2])
            for index in shapely.STRtree(shapes).query(
                footprint, predicate="intersects"
            ):
                region = shapes[index].intersection(footprint)
                if region.area <= 0:
                    continue
                heights = mesh.vertices[mesh.triangles[index], 2]
                interval = HeightInterval(
                    float(heights.min()) + bottom, float(heights.max()) + top
                )
                reservations.append(
                    Reservation(
                        source["source_id"], region, interval, "transport_structure"
                    )
                )
                cells.append(
                    {
                        "source_id": source["source_id"],
                        "purpose": "transport_structure",
                        "component": name,
                        "xy": shapely.to_geojson(region),
                        "bounds_m": [interval.lower_m, interval.upper_m],
                    }
                )
    plan = Plan(
        LocalFrame("ENU:48.21:16.37"),
        reservations=tuple(reservations),
        meshes=(model["top"], model["ground"], model["walls"], model["roof"]),
    )
    binding = {
        "frame": "ENU:48.21:16.37",
        "source_ids": report["used_source_ids"],
        "cells": cells,
        "roof_thickness_m": ROOF_M,
        "source_input_sha256": PIN,
        "scope": "Existing Plan/Reservation types. Conservative per-face circulation/structure bounds; selected terrain reference and actual lining meshes remain separate. No full-world terrain/collision or unknown building-floor clearance claim.",
    }
    return model, report, plan, binding, solid, materials


def source_usage(fixture, report):
    captured = json.loads(BRIDGE_FIXTURE.read_text())
    selected = {s["source_id"]: s for s in captured["sources"]}
    used = {
        s["source_id"]
        for s in captured["sources"]
        if s["role"] in ("deck", "approach_ground", "ground")
    }
    used |= set(report["used_source_ids"])
    rows = []
    for entry in captured["inputs"]:
        for identity in entry["all_axis_ids"]:
            source = selected.get(identity)
            reason = (
                ""
                if identity in used
                else (
                    "buildings_floor_surface_not_planned"
                    if source and source["role"] == "deferred"
                    else source["omission_reason"]
                    if source
                    else "outside_connected_transport_field"
                )
            )
            rows.append(
                {
                    "source_id": identity,
                    "status": "used" if identity in used else "not_used",
                    "reason": reason,
                }
            )
        rows.extend(
            {
                "source_id": identity,
                "status": "not_used",
                "reason": "polygon_source_has_no_axis_recipe",
            }
            for identity in entry["polygon_ids"]
        )
    if len({r["source_id"] for r in rows}) != len(rows) or any(
        r["status"] == "not_used" and not r["reason"] for r in rows
    ):
        raise ValueError("complete unique per-source usage required")
    return rows


def worker(method):
    resource.setrlimit(resource.RLIMIT_CPU, (15, 15))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    fixture, previous = inputs()
    complete(fixture, previous, method)
    samples, hashes = [], []
    for _ in range(5):
        began = time.perf_counter()
        _model, _report, _plan, _binding, solid, _materials = complete(
            fixture, previous, method
        )
        samples.append((time.perf_counter() - began) * 1000)
        hashes.append(
            hashlib.sha256(
                solid.vertices.tobytes() + solid.triangles.tobytes()
            ).hexdigest()
        )
    print(
        json.dumps(
            {
                "method": method,
                "samples_ms": samples,
                "mesh_hashes": hashes,
                "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
                * 1024,
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("parabolic", "cubic"))
    args = parser.parse_args()
    if args.worker:
        worker(args.worker)
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    fixture, previous = inputs()
    rows = []
    for method in ("parabolic", "cubic"):
        fresh = []
        for _ in range(3):
            result = subprocess.run(
                [sys.executable, __file__, "--worker", method],
                capture_output=True,
                text=True,
                timeout=40,
                check=True,
                env=os.environ | {"OPENBLAS_NUM_THREADS": "1", "OMP_NUM_THREADS": "1"},
            )
            fresh.append(json.loads(result.stdout))
        hashes = {h for r in fresh for h in r["mesh_hashes"]}
        if len(hashes) != 1:
            raise ValueError("fresh worker geometry differed")
        _model, report, _plan, binding, solid, materials = complete(
            fixture, previous, method
        )
        usage = source_usage(fixture, report)
        np.savez_compressed(
            OUTPUT / f"{method}-products.npz",
            vertices=solid.vertices,
            triangles=solid.triangles,
            materials=materials,
        )
        for name, value in (
            ("plan", report),
            ("spatial-products", binding),
            ("source-usage", usage),
        ):
            (OUTPUT / f"{method}-{name}.json").write_text(
                json.dumps(value, indent=2) + "\n"
            )
        rows.append(
            {
                "method": method,
                "p50_ms": float(np.median([x for r in fresh for x in r["samples_ms"]])),
                "peak_rss_bytes": max(r["peak_rss_bytes"] for r in fresh),
                "workers": fresh,
                "faces": len(solid.triangles),
                "vertices": len(solid.vertices),
                "used_sources": sum(r["status"] == "used" for r in usage),
                "total_sources": len(usage),
            }
        )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    (OUTPUT / "comparison.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "methods": rows,
                "scope": "Offline recipe, closed bodies and existing spatial products; inputs/imports outside timed samples, complete worker peak RSS. No serialization/render/frame/device claim.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps(rows))


if __name__ == "__main__":
    main()
