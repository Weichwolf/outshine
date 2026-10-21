"""Keep original visible details; compare emitted outer geometry against full images."""

import argparse
import json
import time
from pathlib import Path
from types import MappingProxyType
from unittest.mock import patch

from evidence import revision
from outshine_ref.buildings.modelling.prism_union import frozen
from outshine_ref.buildings.rooflight_panels import triangle_surface
from test_window_detail_demand import cases
from window_detail_demand import WindowDetails, WindowPatch, materialize, window_patch
from window_detail_images import camera_candidates, export, views
from window_extent_plan import compile_bounds
from window_geometry_plan import compile_geometry
from window_geometry_templates import load, save


def draw_patch(asset, identifier):
    raw = asset.emit(identifier)
    mesh = triangle_surface(raw, 10)
    if len(mesh["faces"]) != len(raw["faces"]):
        raise ValueError("Candidate dropped a closed triangle")
    mesh["materials"] = raw["materials"]
    return WindowPatch(
        identifier, MappingProxyType({k: frozen(v) for k, v in mesh.items()})
    )


def probe(output, reference, name, prepared, proof):
    start = time.perf_counter()
    asset = compile_geometry(prepared)
    compile_seconds = time.perf_counter() - start
    save(asset, output / name / "asset")
    start = time.perf_counter()
    restored = load(output / name / "asset", asset.source_id, asset.recipe)
    read_seconds = time.perf_counter() - start
    metadata = compile_bounds(prepared)
    jobs, rows = [], []
    for pose, view in views(prepared).items():
        visible = frozenset(camera_candidates(metadata, view))
        canonical = []

        def mixed(current, window, selected=visible, emitted=canonical, **options):
            if window.identifier in selected:
                return window_patch(current, window, **options)
            emitted.append(window.identifier)
            return draw_patch(restored, window.identifier)

        product = WindowDetails(prepared, max_windows=8192)
        with patch("window_detail_demand.window_patch", mixed):
            start = time.perf_counter()
            draw = product.update(asset.identifiers)
            seconds = time.perf_counter() - start
        mesh = materialize(draw)
        baseline = json.loads(
            (reference / name / pose / "full/scene-geometry.json").read_text()
        )
        if baseline["source_id"] != asset.source_id:
            raise ValueError("Image reference belongs to another source")
        geometry = export(
            output / name / pose, mesh, prepared, view, baseline["ground_span_m"], proof
        )
        jobs.append(
            {
                "case": name,
                "pose": pose,
                "geometry": geometry,
                "reference": str(reference / name / pose / "full/scene-geometry.json"),
            }
        )
        rows.append(
            {
                "pose": pose,
                "original_visible": len(visible),
                "canonical_outer": len(canonical),
                "canonical_ids": canonical,
                "mixed_draw_seconds": seconds,
                "triangles": len(mesh["faces"]),
            }
        )
    return {
        "case": name,
        "source_id": asset.source_id,
        "windows": len(asset.identifiers),
        "templates": len(asset.templates),
        "asset_array_bytes": asset.array_bytes,
        "compile_seconds": compile_seconds,
        "read_seconds": read_seconds,
        "rows": rows,
    }, jobs


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the geometry experiment before evidence")
    args.output.mkdir(parents=True, exist_ok=False)
    plans = cases()
    records, jobs = [], []
    for name, i in (("courts", 0), ("concave", 5), ("compact", 8)):
        row, work = probe(args.output, args.reference, name, plans[i], proof)
        records.append(row)
        jobs.extend(work)
    (args.output / "report.json").write_text(
        json.dumps(
            proof
            | {
                "records": records,
                "jobs": jobs,
                "scope": "Finite static mixed-geometry image control. Visible windows use original draw builder; outer geometry uses saved complete templates. Same opaque UV-independent fixture roles, no general UV/alpha/transmission/reflection/native transform contract. All geometry materialized for the offline renderer; descriptor bytes are not measured GPU residency. Each pose uses a new diagnostic manager; no dynamic residency claim.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps({"producer": proof["commit"], "jobs": len(jobs)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    run(parser.parse_args())
