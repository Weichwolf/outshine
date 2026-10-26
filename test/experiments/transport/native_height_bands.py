"""Compare native indexed height-band cuts with the unchanged tread reference (#1189)."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from mvt_independent_flight_body import construct
from native_planar_mesh import ROOT, build_probe
from stair_indexed_levels import partition
from stair_junction_treads import stair_recipes, unique_caps

UNITS = (
    "HeightBandMesh.cpp",
    "HeightBandClipping.cpp",
    "HeightBandEdges.cpp",
    "HeightBandValidation.cpp",
)


def selectors(plan, heights):
    faces, owners = unique_caps(plan)
    recipes, levels = stair_recipes(plan, heights)
    ranges = []
    for face, owner in zip(faces, owners, strict=True):
        chosen = [r for r in recipes if int(owner) & (1 << r["owner"])]
        if not chosen:
            ranges.append((0, 0))
            continue
        if len(chosen) != 1:
            raise ValueError("one stair recipe per cap required")
        recipe = chosen[0]
        z = heights[face]
        if z.min() < recipe["low"] - 1e-7 or z.max() > recipe["high"] + 1e-7:
            raise ValueError("stair envelope leaves its landing interval")
        if np.ptp(z) < 1e-10:
            step = int(
                np.clip(
                    np.ceil((z.mean() - recipe["low"]) / recipe["rise_m"] - 1e-8),
                    0,
                    recipe["risers"],
                )
            )
            ranges.append((recipe["level_offset"] + step, 1))
        else:
            if int(owner) != 1 << recipe["owner"]:
                raise ValueError("a shared landing must be flat")
            ranges.append((recipe["level_offset"], recipe["risers"] + 1))
    return faces, owners, np.asarray(ranges), levels


def native(binary, output, points, faces, ranges, levels):
    output.mkdir()
    source, result = output / "input.txt", output / "output.txt"
    with source.open("w") as stream:
        stream.write(f"{len(points)} {len(faces)} {len(levels)}\n")
        for point in points:
            stream.write(" ".join(format(float(v), ".17g") for v in point) + "\n")
        for level in levels:
            stream.write(format(float(level), ".17g") + "\n")
        for face, selected in zip(faces, ranges, strict=True):
            stream.write(" ".join(str(int(v)) for v in (*face, *selected)) + "\n")
    process = subprocess.run(
        [str(binary), str(source), str(result)],
        capture_output=True,
        text=True,
        check=True,
    )
    rows = result.read_text().splitlines()
    point_count, face_count = map(int, rows[0].split())
    if not (0 <= point_count <= 65536 and 0 <= face_count <= 8192):
        raise ValueError("bounded native band mesh required")
    if len(rows) != 1 + point_count + face_count:
        raise ValueError("complete native band mesh required")
    xyz = np.asarray(
        [list(map(float, row.split())) for row in rows[1 : 1 + point_count]]
    ).reshape(-1, 3)
    records = np.asarray(
        [list(map(int, row.split())) for row in rows[1 + point_count :]], dtype=int
    ).reshape(-1, 5)
    if face_count and not (
        0 <= records[:, 3].min() <= records[:, 3].max() < len(faces)
    ):
        raise ValueError("each output face must retain its source face")
    return xyz, records[:, :3], records[:, 3], records[:, 4], json.loads(process.stdout)


def consume(binary, output, plan, heights):
    reference = partition(plan, heights)
    faces, owners, ranges, levels = selectors(plan, heights)
    xyz, cut, parents, steps, work = native(
        binary, output, np.c_[plan["xy"], heights], faces, ranges, levels
    )
    np.testing.assert_array_equal(xyz.view(np.uint64), reference[0].view(np.uint64))
    np.testing.assert_array_equal(cut, reference[1])
    np.testing.assert_array_equal(owners[parents], reference[2])
    np.testing.assert_array_equal(steps, reference[3])
    np.testing.assert_array_equal(levels, reference[4])
    all_bands = sum(max(0, int(count) - 1) for _first, count in ranges)
    if all_bands != work["band_visits"] + work["skipped_bands"]:
        raise ValueError(
            "selected and skipped bands must account for the original range visits"
        )
    return (xyz, cut, owners[parents], steps, levels, reference[5]), dict(
        work, all_band_visits=all_bands, geometry_words_equal=True
    )


def fragmented_plan():
    xy = np.asarray([[x, y] for x in range(65) for y in (0.0, 1.0)], dtype=float)
    faces = []
    for x in range(64):
        a = 2 * x
        faces.extend([[a, a + 2, a + 3], [a, a + 3, a + 1]])
    return {
        "xy": xy,
        "parts": [np.asarray(faces)],
        "nodes": np.asarray([[0.0, 0.0], [64.0, 0.0]]),
        "stairs": [(0, 0, 1)],
    }, xy[:, 0] * 0.178


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=UNITS,
        probe_source=ROOT / "test/harness/spatial/HeightBandProbe.cpp",
    )
    reports = []
    for name in ("stacked", "crossed"):
        case = construct(name)
        plan, heights = case["solver_plan"], case["solver_heights"]
        from stair_envelope_remesh import coarsen_envelope

        plan, _ = coarsen_envelope(plan, heights)
        _result, work = consume(binary, args.output / name, plan, heights)
        reports.append(dict(case=name, **work))
    plan, heights = fragmented_plan()
    _result, work = consume(binary, args.output / "fragmented", plan, heights)
    reports.append(dict(case="fragmented_affine_control", **work))
    (args.output / "result.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
