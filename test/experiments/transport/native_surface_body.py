"""Consume one native request-to-body call through the physical reference consumers."""

import argparse
import json
import shlex
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from mvt_independent_flight_body import construct
from native_height_bands import UNITS as CUT_UNITS
from native_height_solid import UNITS as SOLID_UNITS
from native_planar_mesh import ROOT, build_probe
from native_planar_regions import UNITS as REGION_UNITS
from outshine_ref.contracts import Mesh

UNITS = (
    "SurfaceBody.cpp",
    "SurfaceBodyValidation.cpp",
    "SurfaceBodyPlanning.cpp",
    "SurfaceBodyGroups.cpp",
    "SurfaceBodyRemesh.cpp",
    *REGION_UNITS,
    *CUT_UNITS,
    *SOLID_UNITS,
)


def write_input(path, plan, heights):
    flights = {flight["owner"]: flight for flight in plan["stair_lofts"]}
    with path.open("w") as stream:

        def line(values):
            stream.write(" ".join(format(v, ".17g") for v in values) + "\n")

        line(
            (
                len(plan["xy"]),
                len(plan["rows"]),
                len(plan["nodes"]),
                len(plan["stairs"]),
            )
        )
        for xy, height in zip(plan["xy"], heights, strict=True):
            line((*xy, height))
        for owner, (row, faces) in enumerate(
            zip(plan["rows"], plan["parts"], strict=True)
        ):
            line((int(owner in flights), len(row["xy"]), len(faces)))
            for xy in row["xy"]:
                line(xy)
            for face in faces:
                line(face)
        for xy in plan["nodes"]:
            line(xy)
        for owner, low, high in plan["stairs"]:
            flight = flights[owner]
            line(
                (
                    owner,
                    low,
                    high,
                    plan["stair_directions"][owner],
                    flight["start_cut_m"],
                    flight["end_cut_m"],
                )
            )


def read_output(path):
    rows = iter(path.read_text().splitlines())
    header = next(rows).split()
    if len(header) != 9:
        raise ValueError("complete native body header required")
    points, faces, levels, recipes, vertices, triangles, caps, walls = map(
        int, header[:8]
    )
    if not (
        0 <= points <= 65536
        and 0 <= faces == caps <= 8192
        and 0 <= levels <= 1040
        and 0 <= recipes <= 16
        and 0 <= vertices <= 32768
        and 0 <= triangles <= 65536
        and triangles == 2 * caps + walls
    ):
        raise ValueError("bounded complete native body required")

    def array(count, width, dtype):
        return np.array(
            [next(rows).split() for _ in range(count)], dtype=dtype
        ).reshape(-1, width)

    xyz = array(points, 3, float)
    records = array(faces, 5, np.int64)
    heights = array(levels, 1, float).ravel()
    stairs = array(recipes, 6, float)
    mesh = Mesh(array(vertices, 3, float), array(triangles, 3, np.int64))
    metadata = array(walls, 2, np.int64)
    if next(rows, None) is not None:
        raise ValueError("unexpected trailing native body data")
    data = {
        "reference_vertices": xyz,
        "reference_faces": records[:, :3],
        "face_sources": records[:, 3],
        "face_step": records[:, 4],
        "levels_m": heights,
        "body_vertices": mesh.vertices,
        "body_faces": mesh.triangles,
        "top_faces": mesh.triangles[:caps],
        "bottom_faces": mesh.triangles[caps : 2 * caps, ::-1],
        "wall_faces": mesh.triangles[2 * caps :],
        "wall_riser": metadata[:, 1],
        "stair_owners": stairs[:, 0].astype(int),
        "stair_level_offsets": np.r_[stairs[:, 1], len(heights)].astype(int),
    }
    return mesh, data, stairs, float(header[8])


def consume(binary, directory, plan, heights):
    directory.mkdir()
    source, output = directory / "input.txt", directory / "body.txt"
    write_input(source, plan, heights)
    process = subprocess.run(
        [str(binary), str(source), str(output)],
        capture_output=True,
        text=True,
        check=True,
    )
    body, data, recipes, volume = read_output(output)
    work = json.loads(process.stdout)
    note = dict(
        work,
        risers=int(recipes[:, 2].sum()),
        rise_m=float(recipes[:, 5].max()),
        volume_m3=volume,
        vertices=len(body.vertices),
        faces=len(body.triangles),
    )
    return body, data, note


def compare_coverage(actual, expected):
    for key in ("levels_m", "stair_owners", "stair_level_offsets"):
        np.testing.assert_array_equal(actual[key], expected[key])
    groups = [
        set(zip(data["face_sources"].tolist(), data["face_step"].tolist(), strict=True))
        for data in (actual, expected)
    ]
    if groups[0] != groups[1]:
        raise ValueError("native body changed a source/height-level group")
    maximum_area, maximum_boundary = 0.0, 0.0
    for owner, level in sorted(groups[0]):
        shapes = []
        for data in (actual, expected):
            selected = (data["face_sources"] == owner) & (data["face_step"] == level)
            points = data["reference_vertices"]
            faces = data["reference_faces"][selected]
            polygons = shapely.polygons(points[faces, :2])
            shape = shapely.union_all(polygons)
            if abs(float(shapely.area(polygons).sum()) - shape.area) > 1e-7:
                raise ValueError(
                    "native/reference group contains overlapping cap faces"
                )
            shapes.append(shape)
        area = shapes[0].symmetric_difference(shapes[1]).area
        boundary = shapes[0].boundary.hausdorff_distance(shapes[1].boundary)
        if area > 1e-7 or boundary > 1e-8:
            raise ValueError("native body changed source/level coverage or boundary")
        maximum_area = max(maximum_area, area)
        maximum_boundary = max(maximum_boundary, boundary)
    return {
        "source_level_groups": len(groups[0]),
        "maximum_group_area_error_m2": maximum_area,
        "maximum_group_boundary_error_m": maximum_boundary,
    }


def run(args):
    args.output.mkdir()
    compile_dir = args.output / "build"
    compile_dir.mkdir()
    libraries = shlex.split(
        subprocess.check_output(["pkg-config", "--libs", "geos"], text=True)
    )
    binary = build_probe(
        compile_dir,
        units=UNITS,
        probe_source=ROOT / "test/harness/spatial/SurfaceBodyProbe.cpp",
        libraries=libraries,
    )
    reports = []
    for name in ("stacked", "crossed"):
        baseline = construct(name)
        directory = args.output / name

        def build(plan, heights, *, destination=directory):
            return consume(binary, destination, plan, heights)

        with patch("mvt_independent_flight_body.build", side_effect=build):
            result = construct(name)
        reference = baseline["data"]
        same = {
            key: bool(np.array_equal(value, reference[key]))
            for key, value in result["data"].items()
            if key in reference
        }
        coverage = compare_coverage(result["data"], reference)
        np.savez_compressed(directory / "body.npz", **result["data"])
        (directory / "shared-plan.json").write_text(
            json.dumps(result["products"], indent=2) + "\n"
        )
        if args.render:
            from mvt_native_body_display import display

            display(result["plan"], result["data"], directory)
        reports.append(
            {
                "arrangement": name,
                "native": result["geometry"],
                "equal_arrays": same,
                "coverage": coverage,
                "float32_volumes_m3": result["float32_body_volumes_m3"],
                "fixed_profiles": result["fixed_profiles"],
                "scope": "One native body call; fixture height solve and consumer orchestration remain Python.",
            }
        )
    (args.output / "result.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    run(parser.parse_args())
