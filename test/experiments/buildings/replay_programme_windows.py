"""Same requests, eager versus bounded windows with complete facade programmes."""

import argparse
import json
import os
import platform
import resource
import time
from dataclasses import asdict, replace
from pathlib import Path

from compare_window_detail_demand import requests
from evidence import revision
from programme_window_demand import ProgrammeWindowDetails
from programme_window_reference import (
    EagerProgrammeWindows,
    fingerprint,
    source_product,
)
from window_detail_demand import buffer_bytes, materialize

CASES = (
    ("cascade", 0, "cascade", "ribbon-screens"),
    ("paired", 3, "paired-wings", "vertical-slots"),
)


def sequence(product):
    prepared = product.body.facade.source.prepared
    poses, needed = requests(prepared)
    identifiers = tuple(w.identifier for w in prepared.facade.windows)
    return (
        (),
        needed[0],
        needed[0],
        needed[2],
        needed[3],
        identifiers[:12],
        (),
    ), poses[0]


def stage(product, demands, cells, method):
    cpu, wall = time.process_time(), time.perf_counter()
    if method == "eager":
        owner = EagerProgrammeWindows(product, selected_ids=cells)
    else:
        owner = ProgrammeWindowDetails(product, max_windows=8)
    setup = {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
    records = []
    for identifiers in demands:
        began, tick = time.process_time(), time.perf_counter()
        draw = (
            owner.update(identifiers)
            if method == "eager"
            else owner.update(identifiers, selected_ids=cells)
        )
        cost = {
            "cpu_s": time.process_time() - began,
            "wall_s": time.perf_counter() - tick,
        }
        records.append(
            cost
            | {
                "requested_ids": identifiers,
                "detailed_ids": draw.windows.detailed_ids,
                "fallback_ids": draw.windows.fallback_ids,
                "generated_ids": draw.windows.generated_ids,
                "released_ids": draw.windows.released_ids,
                "resident_window_bytes": draw.windows.resident_detail_bytes,
                "generated_window_bytes": draw.windows.generated_buffer_bytes,
                "resident_layer_bytes": draw.resident_layer_bytes,
                "generated_layer_bytes": draw.generated_layer_bytes,
                "spans": len(draw.spans),
                "triangles": sum(s.face_count for s in draw.spans),
            }
        )
    return (
        {
            "cpu_s": time.process_time() - cpu,
            "wall_s": time.perf_counter() - wall,
            "setup": setup,
            "requests": records,
        },
        owner,
        draw,
    )


def validate(product, demands, cells, output):
    eager = EagerProgrammeWindows(product, selected_ids=cells)
    bounded = ProgrammeWindowDetails(product, max_windows=8)
    proofs, first = [], {}
    for index, identifiers in enumerate(demands):
        draws = {
            "eager": eager.update(identifiers),
            "bounded": bounded.update(identifiers, selected_ids=cells),
        }
        meshes = {name: materialize(draw) for name, draw in draws.items()}
        hashes = {name: fingerprint(mesh) for name, mesh in meshes.items()}
        if len(set(hashes.values())) != 1:
            raise ValueError("Same programme/window request changed exact draw buffers")
        arrays = {
            name + "_" + key: value
            for name, mesh in meshes.items()
            for key, value in mesh.items()
        }
        import numpy as np

        np.savez(output / f"request-{index}.npz", **arrays)
        proofs.append(
            {
                "request": index,
                "artifact_sha256": hashes["eager"],
                "fields": len(meshes["eager"]),
            }
        )
        if index == 1:
            first = meshes
    return proofs, first


def images(product, meshes, pose, output, proof):
    from bay_facade_probe import palette as original_palette
    from facade_programme_products import palette
    from industrial_relief_probe import camera
    from podium_probe import write
    from stepped_source_probe import overview

    views = {
        "near": replace(pose, width=1280, height=720),
        "whole": replace(
            overview(meshes["bounded"]["vertices"], [1, -1.25, 0.8]),
            width=1280,
            height=720,
        ),
    }
    jobs = []
    for method, mesh in meshes.items():
        for name, view in views.items():
            path = output / method / name
            path.mkdir(parents=True, exist_ok=False)
            write(
                path,
                "scene",
                mesh,
                {
                    "camera": camera(view),
                    "source_id": product.plan.source_id,
                    "source": {"material_definitions": original_palette() | palette()},
                    "facade_materials": {},
                    "ground_span_m": 10000,
                    "scope": "Same complete source coarse exterior plus identical requested closed windows, programme layers, portal frames and coarse roof. Explicit eight-nearest test demand only; no full interior/visibility/shadow/radiance/native/device admission.",
                },
                proof,
            )
            jobs.append(str((path / "scene-geometry.json").resolve()))
    return jobs


def replay(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the producer before evidence")
    args.output.mkdir(parents=True, exist_ok=False)
    reports, jobs = {}, []
    for name, index, kind, programme in CASES:
        output = args.output / name
        output.mkdir()
        cpu = time.process_time()
        product = source_product(index, kind, programme)
        preparation = time.process_time() - cpu
        facade = product.body.facade.source
        demands, pose = sequence(product)
        cells = tuple(cell.identifier for cell in product.plan.cells[:3])
        runs, traces = {method: [] for method in ("eager", "bounded")}, {}
        for turn in range(args.rounds):
            for method in runs if turn % 2 == 0 else reversed(runs):
                result, _owner, _draw = stage(product, demands, cells, method)
                runs[method].append(result)
                del _owner, _draw
        if args.trace:
            from allocation_measure import allocations

            for method in runs:
                value, traces[method] = allocations(
                    output / (method + ".bin"),
                    lambda method=method, product=product, demands=demands, cells=cells: (
                        stage(product, demands, cells, method)
                    ),
                )
                del value
        parity, meshes = validate(product, demands, cells, output)
        if args.images:
            jobs.extend(images(product, meshes, pose, output / "images", proof))
        reports[name] = {
            "source_id": product.plan.source_id,
            "source_wkb": product.source.plan.body.source_wkb.hex(),
            "core": asdict(product.source.plan.body.core),
            "source_top_m": product.source.plan.source_top_m,
            "body_top_m": product.source.plan.body.top_m,
            "source_preparation_cpu_s": preparation,
            "windows": len(facade.prepared.facade.windows),
            "programme_cells": len(product.plan.cells),
            "requested_cells": cells,
            "portals": len(facade.portals),
            "coarse_buffer_bytes": buffer_bytes(facade.coarse.mesh),
            "physical_facade_unbuilt": facade.near is None
            and product.body.facade.near is None,
            "runs": runs,
            "allocations": traces,
            "validation": parity,
        }
        if not reports[name]["physical_facade_unbuilt"]:
            raise ValueError("Window demand built an unrequested whole physical facade")
    report = proof | {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "cases": reports,
        "render_jobs": jobs,
        "process_peak_rss_kib_before_render": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "scope": "Warm completed source/compact programme inputs; source preparation measured separately. Eager prebuilds/retains all windows, bounded creates only missing demanded windows with8resident slots. Same7requests/cells/complete coarse coverage/portal/coarse roof. Timers include controller setup, metadata cap filtering, window/layer generation and request records, exclude diagnostic materialization/hash/export/render, teardown and profiler processing. Memray covers the same stage with live final ownership; allocation bytes are not copied bytes. Shared coarse arrays counted once per source; total copied bytes unmeasured. No whole physical interior, general visibility/radiance/LOD, DB/IO/cache/native/device/frame proof.",
    }
    (args.output / "report.json").write_text(
        json.dumps(
            report,
            indent=2,
            default=lambda v: v.hex() if isinstance(v, bytes) else v.tolist(),
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                name: {
                    method: [round(v["cpu_s"] * 1000, 3) for v in case["runs"][method]]
                    for method in case["runs"]
                }
                for name, case in reports.items()
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--images", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
