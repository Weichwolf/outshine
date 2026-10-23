"""Compare station lineage with a supplied frozen loft module on retained inputs."""

import argparse
import copy
import importlib.util
import json
import resource
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from mvt_t_stair_loft import prepare_stair_lofts
from scipy.optimize import linprog
from stair_junction_solve import Conditions, overlap_conditions
from station_overlap_case import prepared


def measure(original, operation):
    times = []
    for _ in range(3):
        plan = copy.deepcopy(original)
        began = time.perf_counter()
        operation(plan)
        times.append(time.perf_counter() - began)
    system = Conditions(len(plan["xy"]))
    began = time.perf_counter()
    checks = overlap_conditions(plan, system, 2.82)
    overlap_seconds = time.perf_counter() - began
    matrix = system.matrix()
    statuses = {}
    if len(plan["rows"]) == 4:
        upper = np.unique(plan["parts"][-1])
        plane = plan["xy"] @ [0.01, -0.02]
        for name, gap in (("valid", 3), ("inverted", -3)):
            heights = plane.copy()
            heights[upper] += gap
            answer = linprog(
                np.zeros(len(heights)),
                A_ub=matrix,
                b_ub=system.bounds,
                bounds=list(zip(heights, heights, strict=True)),
                method="highs-ds",
            )
            statuses[name] = {"success": bool(answer.success), "status": answer.status}
    traced = copy.deepcopy(original)
    tracemalloc.start()
    operation(traced)
    _live, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return (
        plan,
        system,
        {
            "prepare_seconds": times,
            "prepare_median_seconds": float(np.median(times)),
            "overlap_assembly_seconds": overlap_seconds,
            "overlap_conditions": checks,
            "sparse_matrix_bytes": sum(
                a.nbytes for a in (matrix.data, matrix.indices, matrix.indptr)
            ),
            "retained_array_bytes": plan["xy"].nbytes
            + sum(a.nbytes for a in plan["parts"] + plan["part_global_faces"]),
            "prepare_traced_peak_bytes": peak,
            "vertices": len(plan["xy"]),
            "refinement": plan["station_refinement"],
            "fixed_affine_plane_highs": statuses,
            "copy_bytes_and_allocation_count": None,
        },
    )


def display(cases, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Line3DCollection, Poly3DCollection

    figure = plt.figure(figsize=(12, 6))
    for at, (name, plan, system) in enumerate(cases):
        axes = figure.add_subplot(1, 2, at + 1, projection="3d")
        owner = plan["stairs"][0][0]
        base = plan["xy"] @ [0.01, -0.02]
        heights = base.copy()
        heights[np.unique(plan["parts"][-1])] += 3
        xyz = np.c_[plan["xy"], heights]
        for source, color in (
            (owner, "lightgray"),
            (len(plan["rows"]) - 1, "lightblue"),
        ):
            axes.add_collection3d(
                Poly3DCollection(
                    xyz[plan["parts"][source]],
                    facecolors=color,
                    edgecolors="gray",
                    linewidths=0.4,
                    alpha=0.7,
                )
            )
        matrix = system.matrix().tocsr()
        pairs = [
            matrix.indices[matrix.indptr[i] : matrix.indptr[i + 1]]
            for i in range(matrix.shape[0])
        ]
        if pairs:
            axes.add_collection3d(
                Line3DCollection(
                    [xyz[p] for p in pairs], colors="darkgreen", linewidths=0.5
                )
            )
        points = xyz[
            np.unique(np.r_[plan["parts"][owner].ravel(), plan["parts"][-1].ravel()])
        ]
        low, high = points.min(axis=0), points.max(axis=0)
        axes.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="east / m",
            ylabel="north / m",
            zlabel="height / m",
            title=f"{name}: {len(system.bounds)} height-order rows",
        )
        axes.set_box_aspect(high - low)
        axes.view_init(elev=24, azim=-55)
    figure.suptitle(
        "Explicit independent upper plane over retained stair footprint; operator diagnostic"
    )
    figure.tight_layout()
    figure.savefig(output / "lineage.png", dpi=150)
    plt.close(figure)


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    spec = importlib.util.spec_from_file_location("baseline_loft", args.baseline_loft)
    baseline = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(baseline)
    results, pictures = {}, []
    for name, angle, shift, independent, permute in (
        ("original", 0, (0, 0), False, False),
        ("layered", 0, (0, 0), True, False),
        ("permuted", 0, (0, 0), True, True),
        ("rigid", -0.83, (100000, -120000), True, True),
    ):
        original, _contacts = prepared(
            angle, shift, independent=independent, permute=permute
        )
        before = overlap_conditions(original, Conditions(len(original["xy"])), 0)
        pairs = []
        results[name] = {"conditions_before_cut": before}
        for label, operation in (
            ("baseline", baseline.prepare_stair_lofts),
            ("corrected", prepare_stair_lofts),
        ):
            plan, system, report = measure(original, operation)
            results[name][label] = report
            pairs.append(plan)
            arrays = {
                "xy": plan["xy"],
                "condition_rows": np.array(system.rows),
                "condition_columns": np.array(system.columns),
                "condition_values": np.array(system.values),
                "condition_bounds": np.array(system.bounds),
            }
            for i, (faces, ids) in enumerate(
                zip(plan["parts"], plan["part_global_faces"], strict=True)
            ):
                arrays[f"faces_{i}"] = faces
                arrays[f"planar_ids_{i}"] = ids
            np.savez_compressed(args.output / f"{name}-{label}.npz", **arrays)
            if name == "layered":
                pictures.append((label, plan, system))
        if not independent:
            results[name]["geometry_and_ids_bit_equal"] = all(
                np.array_equal(a, b)
                for a, b in zip(
                    [
                        pairs[0]["xy"],
                        *pairs[0]["parts"],
                        *pairs[0]["part_global_faces"],
                    ],
                    [
                        pairs[1]["xy"],
                        *pairs[1]["parts"],
                        *pairs[1]["part_global_faces"],
                    ],
                    strict=True,
                )
            )
            assert results[name]["geometry_and_ids_bit_equal"]
        else:
            assert results[name]["baseline"]["overlap_conditions"] == 0
            corrected = results[name]["corrected"]
            assert corrected["overlap_conditions"] > before > 0
            assert corrected["fixed_affine_plane_highs"]["valid"]["success"]
            assert not corrected["fixed_affine_plane_highs"]["inverted"]["success"]
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cases": results,
        "process_peak_rss_bytes_before_plot": int(
            rss if sys.platform == "darwin" else rss * 1024
        ),
        "scope": "Bounded Python operator comparison. Traced preparation excludes input deepcopy and plotting; three untraced timings under current host load. Conditions/HiGHS checks use explicit affine height planes, not an additional full body recipe or native device evidence. Retained array bytes exclude source/domain/Python object storage; peak RSS includes imports and retained comparison products. Allocation count and total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(pictures, args.output)
    print(
        json.dumps(
            {
                "cases": {
                    k: {
                        label: v[label]["overlap_conditions"]
                        for label in ("baseline", "corrected")
                    }
                    for k, v in results.items()
                },
                "original_bit_equal": results["original"]["geometry_and_ids_bit_equal"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-loft", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
