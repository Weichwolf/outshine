"""Construct source ribbons natively between unchanged complete native profiles (#1189)."""

import argparse
import copy
import json
import subprocess
import time
from collections import defaultdict

import numpy as np
import shapely
from mvt_native_cut_domains import cut_ribbon
from mvt_native_cut_profiles import cut_profiles
from mvt_stair_native_crosscuts import FIXTURE
from native_planar_mesh import ROOT, build_probe


def write_input(path, axis, first, last):
    profiles = [np.asarray(p["profile_xyz_m"])[:, :2] for p in (first, last)]
    with path.open("w") as stream:
        stream.write(
            f"{len(axis)} {first['source_station_m']:.17g} {first['axis_vertex']} {len(profiles[0])} "
            f"{last['source_station_m']:.17g} {last['axis_vertex']} {len(profiles[1])}\n"
        )
        for points in (axis, *profiles):
            for point in points:
                stream.write(" ".join(format(float(v), ".17g") for v in point) + "\n")


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=("SurfaceRibbon.cpp", "GeosPolygon.cpp"),
        probe_source=ROOT / "test/harness/spatial/SurfaceRibbonProbe.cpp",
        libraries=subprocess.check_output(
            ["pkg-config", "--libs", "geos"], text=True
        ).split(),
    )
    groups = defaultdict(list)
    for profile in cut_profiles(json.loads(FIXTURE.read_text())):
        groups[profile["source_id"]].append(profile)
    results = []
    controls = [
        ("original", 0, [0, 0]),
        ("translate", 0, [800, -300]),
        ("rotate", 0.37, [0, 0]),
    ]
    for key, profiles in groups.items():
        profiles.sort(key=lambda p: p["source_station_m"])
        if len(profiles) != 2:
            raise ValueError("two complete supplied profiles per source required")
        for name, angle, translation in controls:
            matrix = np.array(
                [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
            )
            transformed = copy.deepcopy(profiles)
            axis = (
                np.asarray(profiles[0]["original_source_xy_m"]) @ matrix.T + translation
            )
            for profile in transformed:
                xyz = np.array(profile["profile_xyz_m"], dtype=float)
                xyz[:, :2] = xyz[:, :2] @ matrix.T + translation
                profile["profile_xyz_m"] = xyz
            began = time.perf_counter()
            expected_axis, expected_domain, expected_work = cut_ribbon(
                axis, *transformed
            )
            python_ms = 1000 * (time.perf_counter() - began)
            stem = key.replace(":", "-") + "-" + name
            source = args.output / (stem + ".input")
            output = args.output / (stem + ".output")
            write_input(source, axis, *transformed)
            process = subprocess.run(
                [str(binary), str(source), str(output)],
                check=True,
                capture_output=True,
                text=True,
            )
            native_ms = json.loads(process.stdout)["build_ms"]
            rows = iter(output.read_text().splitlines())
            count, ring_count, area, miter = map(float, next(rows).split())
            actual_axis = np.array(
                [list(map(float, next(rows).split())) for _ in range(int(count))]
            )
            actual_ring = np.array(
                [list(map(float, next(rows).split())) for _ in range(int(ring_count))]
            )
            if next(rows, None) is not None:
                raise ValueError("trailing native output")
            expected_ring = np.asarray(expected_domain.exterior.coords)[:-1]
            np.testing.assert_allclose(actual_axis, expected_axis, atol=1e-12, rtol=0)
            np.testing.assert_allclose(actual_ring, expected_ring, atol=1e-12, rtol=0)
            first, last = transformed
            a, b = (np.asarray(p["profile_xyz_m"])[:, :2] for p in (first, last))
            np.testing.assert_array_equal(actual_ring[: len(a)], a)
            begin = len(a) + len(actual_axis) - 2
            np.testing.assert_array_equal(actual_ring[begin : begin + len(b)], b[::-1])
            if (
                abs(area - expected_domain.area) > 1e-7
                or abs(miter - expected_work["maximum_miter"]) > 1e-12
            ):
                raise ValueError(
                    "native area or miter differs from the existing recipe"
                )
            results.append(
                {
                    "source": key,
                    "control": name,
                    "axis_points": len(actual_axis),
                    "boundary_points": len(actual_ring),
                    "area_m2": area,
                    "native_build_ms": native_ms,
                    "python_build_ms": python_ms,
                }
            )
    result = {
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "cases": results,
        "scope": "Whole supplied native profiles and original source axes to native cropped domains. No native profile extraction, landing/section preparation or runtime/world admission.",
    }
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    from pathlib import Path

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
