"""Equal physical rooflight variants on the same prepared real hall/envelope inputs."""

import argparse
import gc
import json
import os
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from evidence import revision
from industrial_hall_shell import cuts as hall_cuts
from industrial_roof_choice import section_for
from industrial_rooflight_probe import storage_bytes
from industrial_rooflights import assemble, plan, templates
from provider_contract import attributes
from tile_roof_envelopes import choose
from verify_industrial_halls import construct, sources, view_for


def measure(hall, envelope, fields, view, method):
    templates.cache_clear()
    start = time.perf_counter()
    result = assemble(hall, envelope, fields, 1, 1, view, method=method)
    cold = (time.perf_counter() - start) * 1000
    values = []
    for _ in range(21):
        start = time.perf_counter()
        result = assemble(hall, envelope, fields, 1, 1, view, method=method)
        values.append((time.perf_counter() - start) * 1000)
    del result
    gc.collect()
    tracemalloc.start()
    mesh, shell, body, added, _ = assemble(
        hall, envelope, fields, 1, 1, view, method=method
    )
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {
        "method": method,
        "cold_ms": cold,
        "warm_median_ms": float(np.median(values)),
        "warm_runs_ms": values,
        "rooflight_fields": len(fields),
        "boolean_prism_operands": len(hall_cuts(hall, 1)) + len(fields),
        "envelope_triangles": len(envelope.mesh["faces"]),
        "shell_triangles": len(shell.mesh["faces"]),
        "rooflight_components": len(added),
        "rooflight_triangles": sum(len(p["faces"]) for p in added),
        "rooflight_transformed_position_normal_array_count": len(added) * 2,
        "rooflight_transformed_position_normal_bytes": sum(
            p[k].nbytes for p in added for k in ("vertices", "normals")
        ),
        "joined_render_triangles": len(mesh["faces"]),
        "joined_render_stream_arrays": len(mesh),
        "joined_render_payload_bytes": sum(a.nbytes for a in mesh.values()),
        "joined_render_storage_bytes": storage_bytes([mesh]),
        "retained_body_rooflight_storage_bytes": storage_bytes([body, *added]),
        "tracemalloc_peak_bytes": peak,
        "boolean_omissions": list(shell.omissions),
        "limits": "Prepared envelope/plan inputs; includes hall/window/door/ramp/gate parts and output join. Known transformed and joined array bytes are not total allocator/copy/Manifold scratch counts. Native heap/work unknown; traced Python/NumPy peak is scoped.",
    }


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the measured implementation")
    cases = []
    for row, neighbors in sources()[:3]:
        owner, _before, _info, hall, *_ = construct(row, neighbors)
        section = section_for(row, choose(row, attributes(row["properties"])))
        fields = plan(hall, section)
        cases.append(
            {
                "source_id": row["id"],
                "measurements": [
                    measure(
                        hall, owner.base.current_envelope, fields, view_for(row), method
                    )
                    for method in ("ring", "bars")
                ],
            }
        )
    report = proof | {
        "scope": "Same sources, cut volumes, plane glazing and physical frame volume; ring vs four-bar components. No frame-time/native/GPU or isolated hardware speedup claim.",
        "host_load_average_before_report": os.getloadavg(),
        "logical_cpus": os.cpu_count(),
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "cases": cases,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(output, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
