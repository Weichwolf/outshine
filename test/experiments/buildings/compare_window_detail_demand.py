"""Equal-output full versus demand-driven window residency on stored source plans."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import shapely
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from terraced_admission_probe import controls
from terraced_facade_details import details
from terraced_facade_prepare import positions, prepare
from terraced_facade_probe import materials
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from window_detail_demand import (
    MeshSpan,
    WindowDetails,
    WindowDraw,
    buffer_bytes,
    materialize,
    window_patch,
)


def fingerprint(mesh):
    return {
        key: hashlib.sha256(value.tobytes()).hexdigest() for key, value in mesh.items()
    }


def requests(prepared):
    windows = prepared.facade.windows
    anchor = next(w for w in windows if w.storey == 1 and w.edge[0] == 0)
    points = positions(
        np.array([[*w.centre, w.bottom_m + w.height_m / 2] for w in windows]),
        prepared.mass.core,
    )
    along = np.asarray(anchor.tangent)
    normal = np.array([along[1], -along[0]])
    poses, selections = [], []
    for offset in (0.0, 0.0, 1.5, 3.0, 3.0, 4.5, -1.5):
        target = np.r_[
            np.asarray(anchor.centre) + offset * along,
            anchor.bottom_m + anchor.height_m / 2,
        ]
        eye = target + np.r_[3.0 * normal - 0.4 * along, 0.35]
        eye, target = positions(np.array([eye, target]), prepared.mass.core)
        poses.append(View(eye, target, width=1920, height=1080, fov_deg=48))
        order = np.argsort(np.linalg.norm(points - eye, axis=1), kind="stable")[:8]
        needed = {windows[i].identifier for i in order}
        selections.append(
            tuple(w.identifier for w in windows if w.identifier in needed)
        )
    return poses, selections


def full_draw(product, resident, identifiers):
    admitted, fallback = product.request(identifiers)
    spans = tuple(s for s in product.base_spans if s.identifier not in admitted)
    spans += tuple(
        MeshSpan(resident[i].mesh, 0, len(resident[i].mesh["faces"]), i)
        for i in admitted
    )
    return WindowDraw(
        product.prepared.mass.identifier,
        spans,
        admitted,
        (),
        (),
        fallback,
        0,
        sum(buffer_bytes(p.mesh) for p in resident.values()),
    )


def stage(call, traced):
    if traced:
        tracemalloc.reset_peak()
        baseline = tracemalloc.get_traced_memory()[0]
    started = time.perf_counter()
    result = call()
    elapsed = time.perf_counter() - started
    return result, {
        "seconds": elapsed,
        "additional_traced_peak_bytes": tracemalloc.get_traced_memory()[1] - baseline
        if traced
        else None,
    }


def exports(output, name, prepared, product, first, last, poses, proof):
    meshes = {"first": materialize(first), "last": materialize(last)}
    for state, mesh in meshes.items():
        np.savez_compressed(output / f"{name}-{state}.npz", **mesh)
    if name == "courts":
        record = {
            "camera": camera(poses[0]),
            "source_id": prepared.mass.identifier,
            "source": {"material_definitions": materials()},
            "facade_materials": {},
            "scope": "Explicit eight-nearest-window diagnostic demand; complete compact exterior retained. No interior, projection/radiance/shadow or native LOD admission.",
        }
        write(output, "Near", meshes["first"], record, proof)
        write(output, "Coarse", product.coarse.mesh, record, proof)
    return {state: fingerprint(mesh) for state, mesh in meshes.items()}


def run_case(output, name, prepared, args, proof):
    poses, selections = requests(prepared)
    if args.trace:
        tracemalloc.start()
    product, base_cost = stage(lambda: WindowDetails(prepared), args.trace)
    if args.method == "all":
        resident, detail_cost = stage(
            lambda: {
                w.identifier: window_patch(prepared, w) for w in prepared.facade.windows
            },
            args.trace,
        )
    else:
        resident, detail_cost = {}, {"seconds": 0.0, "additional_traced_peak_bytes": 0}
    rows, first, last = [], None, None
    for identifiers in selections:
        draw, cost = stage(
            lambda identifiers=identifiers: (
                full_draw(product, resident, identifiers)
                if args.method == "all"
                else product.update(identifiers)
            ),
            args.trace,
        )
        first = draw if first is None else first
        last = draw
        row = asdict(replace_report(draw)) | cost
        rows.append(row)
    traced_resident = tracemalloc.get_traced_memory()[0] if args.trace else None
    if args.trace:
        tracemalloc.stop()
    # Diagnostic copies and the legacy batch control are outside the timers above.
    hashes = exports(output, name, prepared, product, first, last, poses, proof)
    batch_start = time.perf_counter()
    batch = details(prepared.local, prepared.facade, legacy_trim=False)
    batch_seconds = time.perf_counter() - batch_start
    return {
        "name": name,
        "source_id": prepared.mass.identifier,
        "source_wkb_sha256": hashlib.sha256(prepared.mass.source_wkb).hexdigest(),
        "programme": prepared.mass.kind,
        "window_count": len(product.windows),
        "source_holes": len(shapely.from_wkb(prepared.mass.source_wkb).interiors),
        "base": base_cost,
        "base_array_bytes": buffer_bytes(product.coarse.mesh),
        "base_spans": len(product.base_spans),
        "initial_details": detail_cost,
        "initial_generated_windows": len(resident),
        "initial_generated_buffer_bytes": sum(
            buffer_bytes(p.mesh) for p in resident.values()
        ),
        "traced_resident_before_diagnostic_export_bytes": traced_resident,
        "updates": rows,
        "output_hashes": hashes,
        "legacy_batch_only": {
            "seconds": batch_seconds,
            "raw_array_bytes": buffer_bytes(batch),
        },
        "poses": [{"eye": p.eye.tolist(), "target": p.target.tolist()} for p in poses],
    }


@dataclass(frozen=True)
class DrawReport:
    detailed_ids: tuple
    generated_ids: tuple
    released_ids: tuple
    fallback_ids: tuple
    generated_buffer_bytes: int
    resident_detail_bytes: int
    draw_spans: int
    triangles: int


def replace_report(draw):
    return DrawReport(
        draw.detailed_ids,
        draw.generated_ids,
        draw.released_ids,
        draw.fallback_ids,
        draw.generated_buffer_bytes,
        draw.resident_detail_bytes,
        len(draw.spans),
        sum(s.face_count for s in draw.spans),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("all", "selective"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(
        Path("/tmp")
    ) or args.output.resolve() == Path("/tmp"):
        parser.error("Research output requires an explicit directory below /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the experiment before measuring it")
    args.output.mkdir(parents=True, exist_ok=True)
    rows = sources()
    compact = next(row for row in controls() if row["shape"] == "compact")
    plans_start = time.perf_counter()
    cases = [
        ("courts", prepare(plan(rows[0], "cascade"))),
        ("concave", prepare(plan(rows[2], "paired-wings"))),
        ("compact", prepare(plan(compact, "cascade"))),
    ]
    plans_seconds = time.perf_counter() - plans_start
    result = proof | {
        "method": args.method,
        "trace": args.trace,
        "plans_seconds": plans_seconds,
        "cases": [
            run_case(args.output, name, prepared, args, proof)
            for name, prepared in cases
        ],
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Linux Python/i5-1135G7 shared host, one BLAS thread. Plans, common compact construction, initial detail generation and seven explicit demand updates separate. All retains every window once; selective retains requested windows only. Same first/last meshes; no full physical-building control. Legacy batch timing omits placement/draw/splitting. Imports/diagnostic export/legacy batch excluded from primary phase times. Generated bytes count new immutable draw buffers and their freezing copy, not all transient allocations/copy traffic. Tracemalloc omits some native allocations. Previous draws are held through this finite probe, so traced residency includes their referenced retired products. No native/GPU/frame/DB admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"method": args.method, "cases": len(cases), "output": str(args.output)}
        )
    )


if __name__ == "__main__":
    main()
