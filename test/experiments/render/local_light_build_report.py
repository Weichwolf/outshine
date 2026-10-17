"""Plot measured native list construction without conflating it with GPU shading."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from local_light_inputs import ROOT


def report(rows, counts, output):
    samples = {row["samples"] for row in rows}
    timing = {(r["case"], r["mode"], r["method"]): r for r in rows}
    work_by_key = {(r["case"], r["mode"], r["method"]): r for r in counts}
    if len(samples) != 1 or timing.keys() != work_by_key.keys():
        raise ValueError("matching cases and one repetition count required")
    for key, row in timing.items():
        other = work_by_key[key]
        if row["cold"]["work_counters"] or not other["cold"]["work_counters"]:
            raise ValueError("counter-free timing and counter-enabled work required")
        if (
            row["words_sha256"] != other["words_sha256"]
            or row["repacked_sha256"] != other["repacked_sha256"]
        ):
            raise ValueError("identical list and rebase outputs required")
    repetitions = samples.pop()
    cases = list(dict.fromkeys(row["case"] for row in rows))
    methods = ("sort", "scatter", "hierarchy")
    selected = {(r["case"], r["method"]): r for r in rows if r["mode"] == "log"}
    counted = {(r["case"], r["method"]): r for r in counts if r["mode"] == "log"}
    fig, axes = plt.subplots(2, 1, figsize=(14, 9), layout="constrained")
    x = np.arange(len(cases))
    for index, method in enumerate(methods):
        values = [selected[case, method] for case in cases]
        positions = x + (index - 1) * 0.24
        median = np.array([r["warm_ms_p50_p95_p99"][0] for r in values])
        tail = np.array([r["warm_ms_p50_p95_p99"][2] for r in values])
        axes[0].bar(positions, median, width=0.22, label=method)
        axes[0].errorbar(
            positions,
            median,
            yerr=[np.zeros_like(tail), tail - median],
            fmt="none",
            color="black",
            capsize=2,
        )
        work = [
            counted[case, method]["cold"]["cell_tests"]
            + counted[case, method]["cold"]["block_tests"]
            for case in cases
        ]
        axes[1].bar(positions, work, width=0.22, label=method)
    for axis in axes:
        axis.set_xticks(x, cases)
        axis.set_yscale("log")
        axis.grid(axis="y", alpha=0.25)
        axis.legend()
    axes[0].set_ylabel("CPU build ms, p50 bars / p99 whiskers")
    axes[1].set_ylabel("AABB tests, parent + leaf")
    dense = selected["dense", "hierarchy"]
    fig.suptitle(
        "Native Log64 construction: identical original-index CSR bytes, all seven cases",
        fontsize=15,
    )
    fig.supxlabel(
        f"Linux development load; 1 cold + {repetitions - 1} warm rebuilds; hot work counters compiled out. CPU, no GPU builder/upload/frame proof.\n"
        f"8192 lights: own PMR cold peak {dense['cold']['own_peak_bytes']:,} B; unchanged-capacity warm allocations 0. "
        f"Camera repack p50 {dense['repack_ms_p50_p95_p99'][0]:.4f} ms.\n"
        "PMR payload excludes input/stdio/allocator metadata/driver. No general FP32 visibility or Places/device acceptance.",
        fontsize=9,
    )
    fig.savefig(output)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--counts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    report(
        json.loads(args.input.read_text()),
        json.loads(args.counts.read_text()),
        args.output,
    )


if __name__ == "__main__":
    main()
