"""Local geometry-build cost on one saved crown; methods have different surfaces."""

import argparse
import gc
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from colonization import pipe_radii
from limb_experiment import canonical_wood
from limb_paths import dominant_paths
from limb_surface import limb_mesh
from spatial_crown_cost import restore
from wood_mesh import branch_tubes


def sample(builder):
    times = []
    for _ in range(3):
        gc.collect()
        start = time.perf_counter()
        result = builder()
        times.append(time.perf_counter() - start)
    gc.collect()
    tracemalloc.start()
    result = builder()
    current, peak = tracemalloc.get_traced_memory()
    blocks = sum(v.count for v in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    return result, {
        "wall_s": times,
        "median_s": float(np.median(times)),
        "traced_live_bytes": current,
        "traced_peak_bytes": peak,
        "traced_live_blocks": blocks,
    }


def run(source, output):
    crown = restore(source)
    radii = pipe_radii(crown.parents, 0.14)
    paths = dominant_paths(crown.positions, crown.parents, radii)
    old, old_cost = sample(
        lambda: branch_tubes(crown.positions, crown.parents, radii, 8)
    )
    old_cost.update(
        faces=len(old[1]),
        vertices=len(old[0]),
        array_bytes=sum(a.nbytes for a in old),
        closed_components=len(crown.parents) - 1,
        cap_faces=16 * (len(crown.parents) - 1),
        stations=2 * (len(crown.parents) - 1),
        dtypes=[str(a.dtype) for a in old],
    )
    reports = {"independent_segments": old_cost}
    for along in (2, 4):
        (mesh, _), cost = sample(lambda along=along: canonical_wood(crown, along))
        payload = sum(v.nbytes for v in mesh.values())
        cost.update(
            faces=len(mesh["faces"]),
            vertices=len(mesh["vertices"]),
            array_bytes=payload,
            closed_components=len(paths),
            cap_faces=16 * len(paths),
            stations=sum((len(p) - 1) * along + 1 for p in paths),
            curve_and_frame_calls=len(paths),
            canonical_segments=len(crown.parents) - 1,
            output_arrays=len(mesh),
            known_concatenation_read_plus_write_bytes=2 * payload,
            dtypes={key: str(v.dtype) for key, v in mesh.items()},
        )
        reports[f"continuous_along_{along}"] = cost
    failed = []
    for path in paths:
        try:
            limb_mesh(crown.positions, radii, path, 1, 8)
        except ValueError as error:
            failed.append({"first_child": path[1], "reason": str(error)})
    result = {
        "source_identity": crown.identity,
        "limbs": len(paths),
        "rejected_coarse_along_1": failed,
        "methods": reports,
        "scope": "Different surfaces/resolutions: no equal-output speed or optimality claim. Three untraced build times, separate traced live/peak; process RSS separately. Known concatenation traffic is a lower bound; internal allocator and spline work not fully counted.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.output)
