"""Both facade programmes on the complete unchanged, independently audited city."""

import argparse
import hashlib
import json
import resource
import time
from collections import Counter
from pathlib import Path

import numpy as np
from bay_city_checks import inspect
from bay_facade_mesh import expand
from bay_facade_plan import FIELD_MATERIAL, PROGRAMMES, RIM_MATERIAL, plan_bays
from bay_facade_probe import palette
from city_scene import inputs
from demand import View
from evidence import revision
from podium_probe import write
from showcase_tiles import frame_proof
from terraced_city_parts import fingerprint
from terraced_city_reference import admitted, unchanged_modelling
from terraced_crown_plan import plan
from terraced_facade_surface import joined
from terraced_mass_census import source_identity
from terraced_terrace_plan import plan as terrace_plan

DATA = Path(__file__).with_name("data")


def load(base_path, audit_path):
    reference = json.loads(audit_path.read_text())
    unchanged_modelling(reference)
    baseline = json.loads((DATA / "terraced-city-baseline.json").read_text())
    rows, receipts, window = inputs(
        "Paris", json.loads((DATA / "roof-crown-city-window.json").read_text())
    )
    records = admitted(rows, reference, baseline)
    if len(records) != 71 or not all(
        r.get(k) is True
        for r in records.values()
        for k in (
            "passed",
            "full_body_checked",
            "full_draw_checked",
            "roof_components_passed",
        )
    ):
        raise ValueError("Whole facade comparison needs all71 body/roof/draw checks")
    base = json.loads(base_path.read_text())
    unchanged_modelling(base)
    if (
        base["procedures"]["physical_reference_sha256"]
        != hashlib.sha256(audit_path.read_bytes()).hexdigest()
    ):
        raise ValueError("Base city uses a different physical audit")
    if base["source"]["generated"] != 988 or base["source"]["omissions"]:
        raise ValueError(
            "Base city does not contain the complete original source window"
        )
    if base["sources"] != receipts or base["window"] != window:
        raise ValueError("Base city has different source receipts/window")
    if not set(baseline["existing_function_ids"]) <= set(
        base["procedures"]["protected_existing_sources"]
    ):
        raise ValueError("Base city lost a protected functional source")
    demand = {r["id"]: r for r in base["procedures"]["new_sources"]}
    if len(demand) != 71 or set(demand) != set(records):
        raise ValueError("Base city changed the71 proposed source owners")
    path = Path(base["mesh"])
    if hashlib.sha256(path.read_bytes()).hexdigest() != base["mesh_sha256"]:
        raise ValueError("Base city mesh bytes changed")
    with np.load(path, allow_pickle=False) as archive:
        mesh = {key: archive[key] for key in archive.files}
    if fingerprint(mesh) != base["mesh_array_sha256"]:
        raise ValueError("Base city numeric arrays changed")
    if {r["id"] for r in rows if r["layer"] == "building"} != {
        r["id"] for r in reference["original_census"]
    }:
        raise ValueError("Base city source identities changed")
    return base, mesh, rows, records, demand


def compose(base, original, rows, records, demand, programme):
    products, results = [], []
    for row in rows:
        identifier = row["id"]
        if identifier not in records:
            continue
        start = time.perf_counter()
        source = records[identifier]
        crown = plan(row, source["programme"])
        terrace = terrace_plan(crown.body)
        planned = plan_bays(terrace.prepared, programme, portals=terrace.portals)
        plan_seconds = time.perf_counter() - start
        diagnostic_start = time.perf_counter()
        checked = inspect(planned, terrace)
        diagnostic_seconds = time.perf_counter() - diagnostic_start
        decision = demand[identifier]
        # Preserve coloured fields for compact bodies too; only side depth is omitted.
        mode = "outer" if decision["body_expanded"] else "front"
        expansion_start = time.perf_counter()
        mesh, owners = expand(planned, mode=mode)
        expansion_seconds = time.perf_counter() - expansion_start
        if not all(np.isfinite(a).all() for a in mesh.values()):
            raise ValueError("Facade addition has nonfinite draw arrays")
        if len(owners) != len(mesh["faces"]):
            raise ValueError("Facade triangle ownership changed")
        if len(mesh["vertices"]) and (
            mesh["vertices"][:, 2].min() < crown.body.bottom_m
            or mesh["vertices"][:, 2].max() > crown.source_top_m
        ):
            raise ValueError("Facade addition exceeds original source heights")
        if any(source[k] != v for k, v in source_identity(row).items()):
            raise ValueError("Facade expansion mutated source geometry/properties")
        products.append(mesh)
        results.append(
            {
                "id": identifier,
                "programme": programme,
                "body_programme": source["programme"],
                "product": mode,
                "cells": len(planned.cells),
                "rejected": planned.rejected,
                "triangles": len(mesh["faces"]),
                "physical_checks": checked,
                "plan_seconds": plan_seconds,
                "diagnostic_seconds": diagnostic_seconds,
                "expansion_seconds": expansion_seconds,
                "seconds": time.perf_counter() - start,
            }
        )
        print(
            json.dumps(
                {
                    "programme": programme,
                    "source": len(results),
                    "id": identifier,
                    "cells": len(planned.cells),
                }
            ),
            flush=True,
        )
    mesh = joined((original, *products))
    check_prefix(original, mesh)
    if len(results) != 71:
        raise ValueError("Whole facade comparison lost a proposed source")
    record = {
        k: base[k]
        for k in (
            "camera",
            "source",
            "sources",
            "window",
            "facade_materials",
            "ground_span_m",
            "frame",
        )
    }
    record["source"] = dict(
        record["source"],
        material_definitions=dict(record["source"]["material_definitions"]),
    )
    definitions = {str(k): palette()[str(k)] for k in (FIELD_MATERIAL, RIM_MATERIAL)}
    if set(definitions) & (
        set(record["source"]["material_definitions"]) | set(record["facade_materials"])
    ):
        raise ValueError("Private facade roles collide with existing material bindings")
    record["source"]["material_definitions"].update(definitions)
    camera = base["camera"]
    view = View(
        np.asarray(camera["eye"]),
        np.asarray(camera["target"]),
        width=camera["profile"][0],
        height=camera["profile"][1],
        fov_deg=camera["vertical_fov_deg"],
    )
    record |= {
        "frame": frame_proof(mesh, view),
        "procedures": base["procedures"] | {"facade_fields": results},
        "original_prefix_sha256": fingerprint(original),
        "original_mesh_sha256": base["mesh_sha256"],
        "mesh_array_sha256": fingerprint(mesh),
        "scope": "All988 original sources,71 independently audited coupled bodies/roofs and unchanged previous draw prefix. Both facade programmes on actual source walls/openings, independently checked closed additions. Outer/front selected by the existing base-body decision; geometry comparison only, no general LOD/radiance/shadow/native/device admission.",
    }
    return mesh, record


def check_prefix(original, mesh):
    for key, value in original.items():
        actual = mesh[key][: len(value)]
        if actual.dtype != value.dtype or actual.shape != value.shape:
            raise ValueError("Whole-window composition altered original array layout")
        # Bounded diagnostic scratch also distinguishes positive and negative zero.
        a, b = actual.view(np.uint8).reshape(-1), value.view(np.uint8).reshape(-1)
        if any(
            not np.array_equal(a[i : i + 1048576], b[i : i + 1048576])
            for i in range(0, len(a), 1048576)
        ):
            raise ValueError("Whole-window composition altered original numeric arrays")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--audit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a directory under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the whole-city facade producer first")
    base, original, rows, records, demand = load(args.base, args.audit)
    args.output.mkdir(parents=True, exist_ok=True)
    summary = []
    for programme in PROGRAMMES:
        start = time.perf_counter()
        mesh, record = compose(base, original, rows, records, demand, programme)
        name = "CityRibbon" if programme == "ribbon-fields" else "CityBays"
        write(args.output, name, mesh, record, proof)
        summary.append(
            {
                "programme": programme,
                "triangles": len(mesh["faces"]),
                "sources": 988,
                "field_sources": 71,
                "products": dict(
                    Counter(r["product"] for r in record["procedures"]["facade_fields"])
                ),
                "seconds": time.perf_counter() - start,
            }
        )
        del mesh, record
    (args.output / "facade-city.json").write_text(
        json.dumps(
            {
                "commit": proof["commit"],
                "complete": True,
                "outcomes": summary,
                "worker_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
