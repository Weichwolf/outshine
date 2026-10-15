"""Compare prepared species draws on an unchanged real cached Place stand."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.places import axes, ecef, read_camera
from outshine_ref.vegetation import load_species_catalog
from pyproj import Transformer
from species_mixture import HeightMixture, site_uniforms
from species_pool_lookup import MIXED, cell_rows, points

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def geography(report_path, lat, lon):
    report = json.loads(report_path.read_text())
    source = Path(report["source"])
    if (
        digest(source) != report["source_sha256"]
        or not report["pool_membership_equivalence_passed"]
    ):
        raise ValueError("explicit verified geographic research input required")
    codes = tuple(
        sorted(
            f["properties"]["LEVEL3_COD"]
            for f in json.loads(source.read_text())["features"]
        )
    )
    index_path = report_path.parent / "geographic-index.npz"
    index = np.load(index_path, allow_pickle=False)
    values = index["table"][cell_rows(report["index"]["level"], points(lat, lon))]
    pools = {}
    for value in np.unique(values):
        if value == 0:
            regions = ()
        elif value < MIXED:
            regions = (codes[int(value) - 1],)
        else:
            bucket = int(value) - MIXED
            offsets = index["offsets"][bucket : bucket + 2]
            regions = tuple(
                codes[i] for i in index["candidates"][offsets[0] : offsets[1]]
            )
        pools[int(value)] = regions
    return (
        values,
        pools,
        {
            "report_sha256": digest(report_path),
            "index_sha256": digest(index_path),
            "source_sha256": report["source_sha256"],
        },
    )


def prepare(model, pools):
    catalog = load_species_catalog()
    mixtures = []
    for value, regions in pools.items():
        candidates = set(catalog.candidate_ids(regions))
        selected = [
            (catalog.aliases[r["name"]], r["weights"])
            for r in model["species"]
            if catalog.aliases[r["name"]] in candidates
        ]
        if selected:
            mixture = HeightMixture.prepare(
                tuple(r[0] for r in selected),
                model["heights_m"],
                np.array([r[1] for r in selected]).T,
            )
            mixtures.append((value, regions, mixture))
    return mixtures


def choose(mixtures, values, height, uniforms, forest, method):
    result = np.full(len(height), -1, dtype=np.int16)
    groups = np.full(len(height), -1, dtype=np.int16)
    for group, (value, _regions, mixture) in enumerate(mixtures):
        selected = forest & (values == value)
        result[selected] = mixture.choose(
            height[selected], uniforms[selected], method=method
        )
        groups[selected] = group
    return result, groups


def run(sites_path, scene_path, geography_path, model_path, output):
    start = time.perf_counter()
    scene = json.loads(scene_path.read_text())
    camera = read_camera(ROOT / "src/assets/places" / f"{scene['place']}.scenario")
    if camera.scenario_sha256 != scene["scenario_sha256"]:
        raise ValueError("cached stand and actual Place scenario differ")
    sites = np.load(sites_path, allow_pickle=False)
    keys = sites["ids"]
    origin = ecef(camera.origin_latitude_deg, camera.origin_longitude_deg, 0)
    world = (
        sites["positions"]
        @ axes(camera.origin_latitude_deg, camera.origin_longitude_deg)
        + origin
    )
    transform = Transformer.from_crs(4978, 4979, always_xy=True)
    lon, lat, height = (np.asarray(a) for a in transform.transform(*world.T))
    habitat = np.array(
        [r["properties"].get("class") == "wood" for r in scene["stand"]["sources"]]
    )
    forest = habitat[sites["source"]]
    values, pools, geographic_input = geography(geography_path, lat, lon)
    model = json.loads(model_path.read_text())
    mixtures = prepare(model, pools)
    prepared_s = time.perf_counter() - start
    began = time.perf_counter()
    uniforms = site_uniforms(keys, 17)
    hash_s = time.perf_counter() - began
    expected = choose(mixtures, values, height, uniforms, forest, "linear")
    actual = choose(mixtures, values, height, uniforms, forest, "binary")
    np.testing.assert_array_equal(expected, actual)
    timings = {"linear": [], "binary": []}
    for repeat in range(6):
        for method in ("linear", "binary") if repeat % 2 == 0 else ("binary", "linear"):
            began = time.perf_counter()
            selected = choose(mixtures, values, height, uniforms, forest, method)
            timings[method].append(time.perf_counter() - began)
            np.testing.assert_array_equal(selected, expected)
    choice, group = actual
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output / "choices.npz",
        ids=keys,
        height_m=height,
        mixture_index=group,
        species_index=choice,
    )
    recipes = [
        {"pool": value, "regions": regions, "mixture": mixture.recipe()}
        for value, regions, mixture in mixtures
    ]
    summary = []
    for index, entry in enumerate(mixtures):
        for local, taxon in enumerate(entry[2].taxa):
            summary.append(
                {
                    "taxon": taxon,
                    "sites": int(
                        np.count_nonzero((group == index) & (choice == local))
                    ),
                }
            )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        "input_sha256": {
            str(p): digest(p)
            for p in (sites_path, scene_path, geography_path, model_path)
        },
        "geographic_input": geographic_input,
        "sites": len(keys),
        "forest_sites": int(forest.sum()),
        "unsupported_nonforest": int((~forest).sum()),
        "unsupported_forest": int(np.count_nonzero(forest & (choice < 0))),
        "height_range_m": [float(height.min()), float(height.max())],
        "mixtures": recipes,
        "summary": summary,
        "prepared_s": prepared_s,
        "site_hash_s": hash_s,
        "site_seed": 17,
        "site_hash_version": "outshine-species-1",
        "timings_s": timings,
        "peak_process_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": model["scope"]
        + " Geographic pool boundaries are not yet continuously blended. Existing six stand columns/site IDs unchanged; this adds selection data, not geometry, images or native cache admission.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sites": len(keys),
                "mixtures": len(mixtures),
                "unsupported_forest": report["unsupported_forest"],
                "timings_s": timings,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sites", "scene", "geography", "model", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    a = parser.parse_args()
    run(a.sites, a.scene, a.geography, a.model, a.output)
