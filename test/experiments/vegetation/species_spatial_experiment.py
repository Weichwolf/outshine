"""Compare correlated markings on the unchanged real cached Koerbersee stand."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.places import axes, ecef, read_camera
from outshine_ref.vegetation import SpeciesHeightMixture, species_site_uniforms
from scipy.spatial import cKDTree
from species_spatial import FourierMarks

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def choices(report, data, uniforms):
    selected = np.full(len(uniforms), -1, dtype=np.int16)
    for i, entry in enumerate(report["mixtures"]):
        active = data["mixture_index"] == i
        mixture = SpeciesHeightMixture.from_recipe(entry["mixture"])
        selected[active] = mixture.choose(data["height_m"][active], uniforms[active])
    return selected


def neighbours(world, before, after):
    active = np.flatnonzero(before >= 0)
    distance, index = cKDTree(world[active]).query(world[active], k=2, workers=1)
    pairs = active[index[:, 1]]
    near = distance[:, 1] <= 30
    return {
        "pairs_within_30m": int(near.sum()),
        "independent_same_species_fraction": float(
            np.mean(before[active[near]] == before[pairs[near]])
        ),
        "correlated_same_species_fraction": float(
            np.mean(after[active[near]] == after[pairs[near]])
        ),
        "median_nearest_distance_m": float(np.median(distance[:, 1])),
        "scope": "Descriptive actual nearest-neighbour statistic, not ecological ground truth",
    }


def run(sites_path, scene_path, baseline, output, length_m, count, strength):
    start = time.perf_counter()
    source = json.loads(scene_path.read_text())
    report = json.loads((baseline / "comparison.json").read_text())
    before = np.load(baseline / "choices.npz", allow_pickle=False)
    sites = np.load(sites_path, allow_pickle=False)
    np.testing.assert_array_equal(before["ids"], sites["ids"])
    camera = read_camera(ROOT / "src/assets/places" / f"{source['place']}.scenario")
    if camera.scenario_sha256 != source["scenario_sha256"]:
        raise ValueError("cached stand and scenario camera differ")
    world = sites["positions"] @ axes(
        camera.origin_latitude_deg, camera.origin_longitude_deg
    ) + ecef(camera.origin_latitude_deg, camera.origin_longitude_deg, 0)
    local = species_site_uniforms(sites["ids"], report["site_seed"])
    np.testing.assert_array_equal(
        choices(report, before, local), before["species_index"]
    )
    field = FourierMarks.prepare(90217, length_m, count)
    timing = {"stream": [], "matrix": []}
    expected = field.uniforms(world, local, strength, "matrix")
    for repeat in range(6):
        for method in ("stream", "matrix") if repeat % 2 == 0 else ("matrix", "stream"):
            began = time.perf_counter()
            uniforms = field.uniforms(world, local, strength, method)
            timing[method].append(time.perf_counter() - began)
            np.testing.assert_array_equal(uniforms, expected)
    selected = choices(report, before, expected)
    np.testing.assert_array_equal(selected < 0, before["species_index"] < 0)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output / "choices.npz",
        ids=sites["ids"],
        world_m=world,
        height_m=before["height_m"],
        mixture_index=before["mixture_index"],
        species_index=selected,
        uniforms=expected,
    )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "place": source["place"],
        "inputs": {
            str(p): digest(p)
            for p in (
                sites_path,
                scene_path,
                baseline / "comparison.json",
                baseline / "choices.npz",
            )
        },
        "sites": len(local),
        "forest_sites": int(np.sum(selected >= 0)),
        "site_seed": report["site_seed"],
        "length_m": length_m,
        "strength": strength,
        "field": field.recipe(),
        "mixtures": report["mixtures"],
        "exact_stream_matrix_variates": True,
        "timings_s": timing,
        "avoided_phase_matrix_bytes": len(world) * count * 8,
        "neighbours": neighbours(world, before["species_index"], selected),
        "species_counts_before": np.bincount(
            before["species_index"][selected >= 0]
        ).tolist(),
        "species_counts_after": np.bincount(selected[selected >= 0]).tolist(),
        "elapsed_s": time.perf_counter() - start,
        "peak_whole_process_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "Authored 60m/0.85 spatial marking; no ecological calibration, density change, plant geometry, native/cache-format or Place image acceptance. Pointwise target probabilities hold over ideal coefficient/site-random ensemble, not as exact finite-stand quotas. Finite Fourier covariance approximates a Gaussian kernel and retains long-range residuals; quantile taxon order influences adjacency.",
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
                    "elapsed_s",
                    "peak_whole_process_rss_MiB",
                )
            }
        )
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sites", type=Path, required=True)
    p.add_argument("--scene", type=Path, required=True)
    p.add_argument("--baseline", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--length-m", type=float, default=60)
    p.add_argument("--waves", type=int, default=256)
    p.add_argument("--strength", type=float, default=0.85)
    args = p.parse_args()
    run(
        args.sites,
        args.scene,
        args.baseline,
        args.output,
        args.length_m,
        args.waves,
        args.strength,
    )
