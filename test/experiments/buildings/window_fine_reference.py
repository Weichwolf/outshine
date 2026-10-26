"""Existing complete physical-facade reference; kept separate from window-only controls."""

import argparse
import json
import time
from pathlib import Path

from evidence import revision
from terraced_facade_geometry import build_facade
from terraced_facade_probe import facade_draw
from test_window_detail_demand import cases
from window_detail_demand import buffer_bytes
from window_detail_images import export, views


def full_facade(prepared):
    start = time.perf_counter()
    asset = build_facade(prepared.mass, prepared=prepared)
    build_seconds = time.perf_counter() - start
    if tuple(w.identifier for w in asset.facade.windows) != tuple(
        w.identifier for w in prepared.facade.windows
    ):
        raise ValueError("Full facade lost original window identities")
    start = time.perf_counter()
    mesh = facade_draw(asset, prepared.mass)
    draw_seconds = time.perf_counter() - start
    arrays = [
        asset.frames,
        asset.floors.shell.mesh,
        asset.floors.stairs.mesh,
        asset.floors.landing.mesh,
    ]
    return mesh, {
        "physical_build_seconds": build_seconds,
        "draw_conversion_seconds": draw_seconds,
        "raw_arrays_bytes": sum(buffer_bytes(part) for part in arrays),
        "draw_arrays_bytes": buffer_bytes(mesh),
        "triangles": len(mesh["faces"]),
        "window_ids": [w.identifier for w in asset.facade.windows],
        "window_cut_count": len(asset.cuts),
    }


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze full-facade reference before comparison")
    args.output.mkdir(parents=True, exist_ok=False)
    plans = cases()
    records, jobs = [], []
    for name, index in (("courts", 0), ("concave", 5), ("compact", 8)):
        prepared = plans[index]
        mesh, record = full_facade(prepared)
        record |= {"case": name, "source_id": prepared.mass.identifier}
        records.append(record)
        for pose, view in views(prepared).items():
            original = json.loads(
                (args.reference / name / pose / "full/scene-geometry.json").read_text()
            )
            if original["source_id"] != prepared.mass.identifier:
                raise ValueError("Different source in full-facade image control")
            geometry = export(
                args.output / name / pose,
                mesh,
                prepared,
                view,
                original["ground_span_m"],
                proof,
            )
            jobs.append({"case": name, "pose": pose, "geometry": geometry})
    (args.output / "report.json").write_text(
        json.dumps(
            proof
            | {
                "records": records,
                "jobs": jobs,
                "scope": "Unchanged existing full physical facade with floors/stairs, opening CSG and legacy closed trim. Same original mass/windows and material-role bindings. Different geometric content from compact host plus closed-window controls; cost alone is not a same-quality speedup. No native/whole-world/device admission.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps({"producer": proof["commit"], "cases": len(records)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    run(parser.parse_args())
