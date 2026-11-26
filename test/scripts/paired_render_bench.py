#!/usr/bin/env python3
"""Replay the same external scene in the world renderer and the Metal tile renderer."""

import argparse
import json
import math
import sqlite3
import statistics
import tempfile
import xml.etree.ElementTree as ET
from contextlib import closing
from pathlib import Path

from places import ROOT, benchmark_report, keep_latest
from place_turn import turn_axis


RATE = 30
REPEATS = 3
CASES = ("still", "turn", "flight")
ZERO = "0,0,0"


def motion(scenario, case, flight):
    root = ET.parse(scenario).getroot()
    view = root.find("views/view")
    if view is None or view.find("at") is None or root.find("world") is None:
        raise ValueError("paired replay requires a geographic view and world origin")
    angular = ZERO
    if case == "turn":
        axis = turn_axis(root.find("world"), view.find("at"))
        angular = ",".join(format(360 * value, ".17g") for value in axis)
    return view.get("id"), angular, flight if case == "flight" else ZERO


def cache_entries(folder):
    path = folder / "assets.sqlite"
    if not path.is_file():
        return 0
    with closing(sqlite3.connect(path.as_uri() + "?mode=ro", uri=True)) as connection:
        return connection.execute("SELECT count(*) FROM assets").fetchone()[0]


def assert_pair(world, tile):
    if any(world[key] != tile[key] for key in
           ("declaration_sha256", "view", "profile", "backend", "cpu", "gpu")):
        raise ValueError("paired benchmark scene, profile or device changed")
    if world["build"]["commit"] != tile["build"]["commit"]:
        raise ValueError("paired benchmark build changed")
    if (world["backend"], world["profile"]["fps"], world["profile"]["width_px"],
            world["profile"]["height_px"]) != ("Metal", RATE, 1280, 720) or "A18 Pro" not in world["gpu"]:
        raise ValueError("the paired benchmark requires native A18 Pro Metal at 720p30")
    for report, path in ((world, "world"), (tile, "metal_tile")):
        if report["render_path"] != path or report["motion"]["frames"] != RATE:
            raise ValueError("paired benchmark path or frame count changed")
        if not report["measurement_complete"] or not report["gpu_times"]["complete"]:
            raise ValueError(f"{path} is missing a CPU or GPU frame sample")
    for key in ("frames", "time_step_s", "linear_velocity_mps", "angular_velocity_deg_s"):
        if world["motion"][key] != tile["motion"][key]:
            raise ValueError(f"paired benchmark motion differs at {key}")
    for moment in ("start", "finish"):
        a, b = (report["motion"][moment] for report in (world, tile))
        for key in ("perspective_fov_deg", "near_m", "far_m"):
            if a[key] != b[key]:
                raise ValueError(f"paired benchmark lens differs at {moment} {key}")
        if len(a["world_from_camera"]) != 16 or len(b["world_from_camera"]) != 16 or not all(
            math.isclose(x, y, rel_tol=0, abs_tol=1e-6)
            for x, y in zip(a["world_from_camera"], b["world_from_camera"])
        ):
            raise ValueError(f"paired benchmark camera differs at {moment}")


def sample(options, fixture, case, work, assets, trial):
    scenario = fixture["scenario"]
    view, angular, linear = motion(scenario, case, fixture["flight_mps"])
    asset_count = cache_entries(assets)
    state = "warm" if asset_count else "cold"
    folder = work / state
    result = {}
    paths = {"world": folder / "world", "metal_tile": folder / "tile"}
    modes = tuple(paths)
    for mode in (modes if trial % 2 else modes[::-1]):
        world = mode == "world"
        report = benchmark_report(scenario, view, options, paths[mode], RATE, angular, linear,
                                  asset_cache=assets if world else None,
                                  tile_product=None if world else fixture["product"])
        report["cache_setup"] = (
            {"kind": "generated_asset_store", "initial_entries": asset_count, "state": state}
            if world else {"kind": "explicit_prepared_tile", "state": "prepared"}
        )
        if world:
            profile = report["gpu_profile"]
            report["rendered_content"].update(
                draws_max=profile.get("draws_max"), triangles_max=profile.get("triangles_max")
            )
        report_path = paths[mode] / "benchmark.json"
        report_path.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
        result[mode] = report
    assert_pair(result["world"], result["metal_tile"])
    return result, paths


def summarize(reports):
    def values(*keys):
        series = []
        for report in reports:
            value = report
            for key in keys:
                value = value.get(key) if isinstance(value, dict) else None
            series.append(value)
        return statistics.median(series) if all(v is not None for v in series) else None

    pass_names = set().union(*((r["pass_times"]["passes"] or {}).keys() for r in reports))
    return {
        "rendered_content": reports[-1]["rendered_content"],
        "content_complete": all(r["content_complete"] for r in reports),
        "world_complete": all(r["world_complete"] for r in reports),
        "cache_setup": [r["cache_setup"] for r in reports],
        "startup_ms": statistics.median(sum(r["phases_ms"][key] for key in
                                            ("setup", "preload", "path_preparation")) for r in reports),
        "cpu_p50_ms": values("cpu_times", "frame", "p50_ms"),
        "cpu_p99_ms": values("cpu_times", "frame", "p99_ms"),
        "cpu_work_p99_ms": values("cpu_times", "work", "p99_ms"),
        "gpu_p50_ms": values("gpu_times", "frame", "p50_ms"),
        "gpu_p99_ms": values("gpu_times", "frame", "p99_ms"),
        "pass_scope": reports[-1]["pass_times"]["scope"],
        "passes_complete": all(r.get("gpu_profile", {}).get("available", r["gpu_times"]["complete"])
                               for r in reports),
        "pass_p99_ms": {name: values("pass_times", "passes", name, "p99_ms")
                        for name in sorted(pass_names)},
        "peak_footprint_bytes": values("process_memory", "peak_footprint_bytes"),
        "source_delivery_bytes": values("traffic", "source_delivery_bytes"),
        "asset_read_bytes": values("traffic", "asset_read_bytes"),
        "upload_bytes": values("traffic", "upload_bytes"),
        "upload_scope": reports[-1]["traffic"]["upload_scope"],
        "image_variation_min": min(r["image_row_variation"] for r in reports),
    }


def run_case(options, fixture, case):
    reports = {state: {path: [] for path in ("world", "metal_tile")} for state in ("cold", "warm")}
    work = options.work / fixture["scenario"].stem / case
    work.mkdir(parents=True)
    for trial in range(1, REPEATS + 1):
        with tempfile.TemporaryDirectory(prefix="assets-", dir=work) as store:
            assets = Path(store)
            for state, expect_warm in (("cold", False), ("warm", True)):
                if bool(cache_entries(assets)) != expect_warm:
                    raise ValueError(f"{state}: isolated store has an unexpected initial state")
                result, paths = sample(options, fixture, case, work / str(trial), assets, trial)
                for mode, report in result.items():
                    reports[state][mode].append(report)
                if trial == REPEATS and expect_warm:
                    for mode, folder in paths.items():
                        keep_latest(folder / "image.png", options.shots,
                                    f"{fixture['scenario'].stem}-{case}-{mode}")
                print(f"{fixture['scenario'].stem}/{case}/{state} {trial}/{REPEATS}", flush=True)
    return {state: {path: summarize(runs) for path, runs in paths.items()}
            for state, paths in reports.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--shots", type=Path, required=True)
    parser.add_argument("--client", type=Path, default=ROOT / "build/outshine")
    parser.add_argument("--cache-dir", type=Path)
    parser.add_argument("--preload-seconds", type=int, default=240)
    parser.add_argument("--case", choices=CASES, action="append")
    parser.add_argument("--place", action="append")
    options = parser.parse_args()
    options.client = options.client.resolve()
    options.work = options.work.resolve()
    options.bench = True
    options.bench_profile = "a18"
    options.online = False
    options.work.mkdir(parents=True)
    outcomes = {}
    for fixture in json.loads(options.fixtures.read_text()):
        scenario = (options.fixtures.parent / fixture["scenario"]).resolve()
        if options.place and scenario.stem not in options.place:
            continue
        fixture["product"] = (options.products / (scenario.stem + ".json")).resolve()
        fixture["scenario"] = scenario
        for case in options.case or CASES:
            outcomes[f"{scenario.stem}/{case}"] = run_case(options, fixture, case)
            (options.work / "comparison.json").write_text(
                json.dumps(outcomes, indent=2, allow_nan=False) + "\n"
            )
    if not outcomes:
        raise ValueError("no matching paired replay fixtures")


if __name__ == "__main__":
    main()
