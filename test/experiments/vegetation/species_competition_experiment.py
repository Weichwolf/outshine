"""Compare competing spatial taxa with the saved actual-source rank-field plan."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture
from species_competition import CompetingMarks
from species_spatial_experiment import neighbours


def select(report, data, marks, mode="stream"):
    result = np.full(len(data["ids"]), -1, dtype=np.int16)
    for group, entry in enumerate(report["mixtures"]):
        active = data["mixture_index"] == group
        mixture = SpeciesHeightMixture.from_recipe(entry["mixture"])
        result[active] = marks.choose(
            mixture,
            data["world_m"][active],
            data["ids"][active],
            data["height_m"][active],
            report["strength"],
            mode,
        )
    return result


def run(baseline, output):
    start = time.perf_counter()
    report = json.loads((baseline / "comparison.json").read_text())
    data = np.load(baseline / "choices.npz", allow_pickle=False)
    taxa = sorted(
        {taxon for entry in report["mixtures"] for taxon in entry["mixture"]["taxa"]}
    )
    marks = CompetingMarks.prepare(
        taxa, 17, report["length_m"], len(report["field"]["waves_rad_per_m"])
    )
    timing = {"stream": [], "matrix": []}
    expected = select(report, data, marks)
    for repeat in range(3):
        for mode in ("stream", "matrix") if repeat % 2 == 0 else ("matrix", "stream"):
            began = time.perf_counter()
            selected = select(report, data, marks, mode)
            timing[mode].append(time.perf_counter() - began)
            np.testing.assert_array_equal(selected, expected)
    reversed_marks = CompetingMarks(marks.entries[::-1])
    np.testing.assert_array_equal(select(report, data, reversed_marks), expected)
    np.testing.assert_array_equal(expected < 0, data["species_index"] < 0)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output / "choices.npz",
        ids=data["ids"],
        world_m=data["world_m"],
        height_m=data["height_m"],
        mixture_index=data["mixture_index"],
        species_index=expected,
    )
    stats = neighbours(data["world_m"], data["species_index"], expected)
    stats["shared_rank_same_species_fraction"] = stats.pop(
        "independent_same_species_fraction"
    )
    stats["competing_same_species_fraction"] = stats.pop(
        "correlated_same_species_fraction"
    )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "place": report["place"],
        "input_sha256": {
            str(baseline / name): hashlib.sha256(
                (baseline / name).read_bytes()
            ).hexdigest()
            for name in ("comparison.json", "choices.npz")
        },
        "sites": len(data["ids"]),
        "forest_sites": int(np.sum(expected >= 0)),
        "mixtures": report["mixtures"],
        "strength": report["strength"],
        "length_m": report["length_m"],
        "competing_marks": marks.recipe(),
        "timings_s": timing,
        "stream_matrix_choices_exact": True,
        "reversed_taxon_field_order_choices_exact": True,
        "neighbours": stats,
        "species_counts_rank_field": np.bincount(
            data["species_index"][expected >= 0]
        ).tolist(),
        "species_counts_competing_fields": np.bincount(
            expected[expected >= 0]
        ).tolist(),
        "elapsed_s": time.perf_counter() - start,
        "peak_whole_process_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "Independent per-taxon latent fields remove ordinal CDF adjacency. Uncalibrated forest proposal, not ecological truth or actual resource competition. Finite covariance and conditional finite-stand deviations remain. No source sites, input heights/weights, plant geometry, public images or native products changed.",
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "sites",
                    "forest_sites",
                    "neighbours",
                    "timings_s",
                    "elapsed_s",
                    "peak_whole_process_rss_MiB",
                )
            }
        )
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    run(a.baseline, a.output)
