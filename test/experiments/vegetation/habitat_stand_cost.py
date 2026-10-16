"""Exact parent counts versus enumerating the same canonical fine sites; population scaling."""

import argparse
import json
import os
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from habitat_stand_experiment import plans
from outshine_ref.vegetation import HabitatStand
from shapely.geometry.base import BaseGeometry


def coarse(plan):
    keys = [(3, x, y) for x in range(8) for y in range(8)]
    return np.array([plan.field(k)["counts"] for k in keys], np.uint64)


def enumerated(plan):
    keys = [(3, x, y) for x in range(8) for y in range(8)]
    return np.array(
        [
            np.bincount(plan.sites(k)["ids"][:, 3].astype(np.int64), minlength=2)
            for k in keys
        ],
        np.uint64,
    )


def run(output):
    source = list(plans())[1][1]
    load = os.getloadavg()
    rounds = []
    for n in range(6):
        for name, operation in (
            (("parents", coarse), ("all-sites", enumerated))
            if n % 2 == 0
            else (("all-sites", enumerated), ("parents", coarse))
        ):
            p = replace(source, levels=3, counts=(35000, 15000))
            start = time.perf_counter()
            counts = operation(p)
            elapsed = time.perf_counter() - start
            np.testing.assert_array_equal(counts.sum(axis=0), p.counts)
            if name == "parents":
                expected = counts
            else:
                np.testing.assert_array_equal(counts, expected)
            rounds.append(
                {
                    "round": n,
                    "method": name,
                    "seconds": elapsed,
                    "cells": len(p._cells),
                    "sites": p._site_count,
                    "site_array_bytes": sum(
                        a.nbytes
                        for record in p._sites.values()
                        for a in record.values()
                    ),
                    "geometry_wkb_bytes": p._geometry_bytes,
                }
            )
    peaks = {}
    for name, operation in (("parents", coarse), ("all-sites", enumerated)):
        tracemalloc.start()
        p = replace(source, levels=3, counts=(35000, 15000))
        operation(p)
        current, peak = tracemalloc.get_traced_memory()
        snapshot = tracemalloc.take_snapshot()
        tracemalloc.stop()
        peaks[name] = {
            "retained_bytes": current,
            "peak_bytes": peak,
            "retained_blocks": sum(s.count for s in snapshot.statistics("filename")),
        }
    scale = []
    for count in (1000, 1_000_000, 1_000_000_000):
        times = []
        for _ in range(6):
            p = replace(source, counts=(count * 7 // 10, count - count * 7 // 10))
            start = time.perf_counter()
            p.field()
            children = p.children()
            times.append(time.perf_counter() - start)
            np.testing.assert_array_equal(
                np.array([c.counts for c in children], np.uint64).sum(axis=0), p.counts
            )
            assert not p._sites
        scale.append(
            {
                "population": count,
                "median_s": float(np.median(times)),
                "prepared_cells": len(p._cells),
                "prepared_sites": p._site_count,
                "geometry_wkb_bytes": p._geometry_bytes,
            }
        )
    p = source
    counts = coarse(p)
    packet = json.loads(json.dumps(p.recipe()))
    with (
        patch.object(
            HabitatStand, "_split", side_effect=AssertionError("split on hit")
        ),
        patch.object(HabitatStand, "_random", side_effect=AssertionError("RNG on hit")),
        patch.object(
            BaseGeometry, "intersection", side_effect=AssertionError("clip on hit")
        ),
        patch.object(
            shapely, "normalize", side_effect=AssertionError("normalize on hit")
        ),
    ):
        start = time.perf_counter()
        restored = HabitatStand.from_recipe(packet)
        np.testing.assert_array_equal(coarse(restored), counts)
        restore_s = time.perf_counter() - start
    result = {
        "source_id": source.habitat.source_id,
        "benchmark_counts": [35000, 15000],
        "comparison": "Same level3 canonical plan, source support and exact cell/kind counts. Eager path generates every canonical leaf site's XY/IDs before reducing; parent path needs none. This measures avoided fine work, not global optimality.",
        "rounds": rounds,
        "untimed_tracemalloc": peaks,
        "root_first_children_population_scaling": scale,
        "prepared_hierarchy_restore_and_all_coarse_fields_s": restore_s,
        "restored_sites": restored._site_count,
        "serialized_prepared_plan_bytes": len(json.dumps(packet).encode()),
        "load_start": load,
        "load_end": os.getloadavg(),
        "scope": "CPU reference counts/work/array payload and traced live blocks only; no device/frame/RSS guarantee. Counts/mixture are authored, not observed stock. Source geometry and quality requirements fixed between compared paths.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "median_s": {
                    name: float(
                        np.median([r["seconds"] for r in rounds if r["method"] == name])
                    )
                    for name in ("parents", "all-sites")
                },
                "scale": scale,
                "peaks": peaks,
                "restore_s": restore_s,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
