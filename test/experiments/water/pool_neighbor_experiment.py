"""Measure and display the existing planar outline control's shared-edge closure."""

import argparse
import hashlib
import json
import os
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from pool_body_experiment import FIXTURE
from pool_contact_neighbors import edge_profile, reconcile_pair
from pool_region_experiment import controls


def picture(body, source, initial, updated, report, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, (plan, section) = plt.subplots(
        1, 2, figsize=(14, 5), constrained_layout=True
    )
    ring = body["plan_xy"][body["outer_ring"]]
    loop = np.vstack((ring, ring[0]))
    plan.plot(loop[:, 0], loop[:, 1], color="black", label="Existing pool outline")
    for patch, colour in zip(updated, ("#976b37", "#247d94")):
        vertices, faces = patch[:2]
        plan.triplot(vertices[:, 0], vertices[:, 1], faces, color=colour, lw=0.4)
    edge = (0, 3)
    plan.plot(
        source[list(edge), 0], source[list(edge), 1], "r--", label="Shared source edge"
    )
    plan.set(xlabel="East / m", ylabel="North / m", title="Reconciled planar partition")
    plan.set_aspect("equal")
    plan.legend(fontsize=8)
    for i, patch in enumerate(initial):
        profile = edge_profile(source, patch, edge)
        keys = sorted(profile)
        section.plot(
            list(map(float, keys)),
            [profile[t][2] for t in keys],
            "o-",
            label=f"Independent side {i + 1}: {len(keys)} knots",
            markersize=3,
        )
    profile = edge_profile(source, updated[0], edge)
    keys = sorted(profile)
    section.plot(
        list(map(float, keys)),
        [profile[t][2] for t in keys],
        "k.--",
        label=f"Both reconciled sides: {len(keys)} knots",
    )
    section.set(xlabel="Canonical original-edge parameter", ylabel="ENU Up / m")
    section.set_title(
        f"{report['maximum_initial_seam_gap_m']:.3f} m initial seam gap; "
        f"{report['maximum_reconciled_seam_gap_m']:.3g} m after knot exchange"
    )
    section.legend(fontsize=8)
    figure.suptitle(
        "Existing planar geometry control; missing native terrain remains pending"
    )
    figure.savefig(output / "shared-edge.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("bounded repeat count between 1 and 21 required")
    args.output.mkdir(parents=True, exist_ok=False)
    _, body, source, faces = next(
        c for c in controls(FIXTURE) if c[0] == "original_outline_rectangle"
    )
    load = os.getloadavg()
    samples = []
    for _ in range(args.repeats + 1):
        wall, cpu = time.perf_counter(), time.process_time()
        initial, updated, report = reconcile_pair(body, source, faces[:1], faces[1:])
        samples.append(
            {
                "wall_ms": (time.perf_counter() - wall) * 1000,
                "cpu_ms": (time.process_time() - cpu) * 1000,
            }
        )
    tracemalloc.start()
    traced = reconcile_pair(body, source, faces[:1], faces[1:])
    live, peak = tracemalloc.get_traced_memory()
    blocks = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    del traced
    peak_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    linux_hwm = None
    status = Path("/proc/self/status")
    if sys.platform == "linux" and status.exists():
        linux_hwm = next(
            (
                int(line.split()[1]) * 1024
                for line in status.read_text().splitlines()
                if line.startswith("VmHWM:")
            ),
            None,
        )
    for name, patches in (("before", initial), ("after", updated)):
        np.savez(
            args.output / f"{name}.npz",
            source=source,
            source_faces=faces,
            first_vertices=patches[0][0],
            first_faces=patches[0][1],
            second_vertices=patches[1][0],
            second_faces=patches[1][1],
        )
    report.update(
        {
            "input_sha256": hashlib.sha256(
                (FIXTURE / "input.json").read_bytes()
            ).hexdigest(),
            "control": "Existing original_outline_rectangle with declared planar heights, not native DEM.",
            "first_and_following_cost": samples,
            "load_before_measurement": load,
            "tracemalloc_live_bytes": live,
            "tracemalloc_peak_bytes": peak,
            "tracemalloc_live_blocks_not_total_allocations": blocks,
            "geos_heap_included_in_tracemalloc": False,
            "process_peak_rss_before_plot_bytes": peak_rss
            * (1 if sys.platform == "darwin" else 1024),
            "rusage_scope": "Whole process; Linux RUSAGE_SELF may retain pre-exec history. Compare VmHWM or launch in a fresh child.",
            "linux_vm_hwm_before_plot_bytes": linux_hwm,
            "initial": [p[3] for p in initial],
            "updated": [p[3] for p in updated],
            "retained_patch_numeric_bytes": sum(
                p[0].nbytes + p[1].nbytes + p[2].nbytes for p in initial + updated
            ),
        }
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.render:
        picture(body, source, initial, updated, report, args.output)


if __name__ == "__main__":
    main()
