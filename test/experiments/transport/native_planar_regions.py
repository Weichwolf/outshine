"""Consume native envelope/band remeshing in the independent-flight bodies (#1189)."""

import argparse
import json
import subprocess
from collections import defaultdict
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
import stair_band_remesh
import stair_envelope_remesh
import stair_indexed_levels
import stair_junction_body
from mvt_independent_flight_body import construct
from native_height_bands import UNITS as CUT_UNITS
from native_height_bands import consume as consume_cuts
from native_height_solid import UNITS as SOLID_UNITS
from native_height_solid import compare as compare_solid
from native_height_solid import read_output as read_solid
from native_planar_mesh import ROOT, build_probe
from stair_junction_treads import unique_caps

UNITS = (
    "PlanarRegionRemesh.cpp",
    "PlanarRegionRings.cpp",
    "PlanarRegionTriangulation.cpp",
    "GeosPolygon.cpp",
)


def oriented_faces(faces):
    return sorted(
        min(tuple(face), tuple(np.roll(face, 1)), tuple(np.roll(face, 2)))
        for face in faces
    )


def boundary(faces):
    edges = defaultdict(list)
    for face in faces:
        for a, b in zip(face, np.roll(face, -1), strict=True):
            edges[tuple(sorted((a, b)))].append((a, b))
    result = []
    for incidences in edges.values():
        if len(incidences) == 1:
            result.extend(incidences)
        elif len(incidences) != 2 or incidences[0] != incidences[1][::-1]:
            raise ValueError("region has nonmanifold or inconsistently oriented edges")
    return sorted(result)


def equivalent(points, actual, expected):
    if len(actual) != len(expected) or boundary(actual) != boundary(expected):
        raise ValueError("native remeshing changed the indexed boundary or facet count")
    np.testing.assert_array_equal(np.unique(actual), np.unique(expected))
    triangles = shapely.polygons(points[actual, :2])
    footprint = shapely.union_all(triangles)
    reference = shapely.union_all(shapely.polygons(points[expected, :2]))
    if (
        footprint.symmetric_difference(reference).area > 1e-7
        or abs(float(shapely.area(triangles).sum()) - footprint.area) > 1e-7
    ):
        raise ValueError(
            "native remeshing changed coverage or introduced overlapping faces"
        )
    return oriented_faces(actual) == oriented_faces(expected)


def native(binary, directory, points, faces, groups):
    directory.mkdir()
    path = directory / "input.txt"
    with path.open("w") as stream:
        stream.write(f"{len(points)} {len(faces)} {len(groups)}\n")
        for xy in points[:, :2]:
            stream.write(" ".join(format(float(value), ".17g") for value in xy) + "\n")
        for face in faces:
            stream.write(" ".join(str(int(value)) for value in face) + "\n")
        for indices in groups:
            stream.write(f"{len(indices)} " + " ".join(map(str, indices)) + "\n")
    output = directory / "output.txt"
    process = subprocess.run(
        [str(binary), str(path), str(output)],
        capture_output=True,
        text=True,
        check=True,
    )
    rows = iter(output.read_text().splitlines())
    if int(next(rows)) != len(groups):
        raise ValueError("native output must retain every input region")
    rebuilt = []
    total = 0
    for _group in groups:
        count = int(next(rows))
        total += count
        if not 0 < count <= total <= 8192:
            raise ValueError("bounded complete remeshed region required")
        rebuilt.append(
            np.array([list(map(int, next(rows).split())) for _ in range(count)])
        )
    if next(rows, None) is not None:
        raise ValueError("unexpected trailing native output")
    return rebuilt, json.loads(process.stdout)


def compare(binary, directory, points, faces, owners, steps, flights):
    selected, groups = stair_band_remesh._groups(points, faces, owners, steps, flights)
    protected = {tuple(points[v, :2]) for v in np.unique(faces[~selected])}
    actual, work = native(binary, directory, points, faces, list(groups.values()))
    output, masks, bands = faces[~selected].tolist(), owners[~selected].tolist(), []
    if steps is not None:
        bands = steps[~selected].tolist()
    variants = 0
    for (key, indices), rebuilt in zip(groups.items(), actual, strict=True):
        vertices = np.unique(faces[indices])
        lookup = {tuple(points[v, :2]): int(v) for v in vertices}
        shape = shapely.union_all(shapely.polygons(points[faces[indices], :2]))
        expected, _note = stair_band_remesh._triangulate(shape, protected, lookup)
        variants += not equivalent(points, rebuilt, np.asarray(expected))
        output.extend(rebuilt.tolist())
        masks.extend([key[0]] * len(rebuilt))
        if steps is not None:
            bands.extend([key[1]] * len(rebuilt))
    return (
        np.array(output),
        np.array(masks),
        np.array(bands),
        dict(work, cdt_variant_regions=variants),
    )


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    libraries = subprocess.check_output(
        ["pkg-config", "--libs", "geos"], text=True
    ).split()
    binary = build_probe(
        args.output,
        units=UNITS,
        probe_source=ROOT / "test/harness/spatial/PlanarRegionProbe.cpp",
        libraries=libraries,
    )
    solid_directory = args.output / "solid"
    solid_directory.mkdir()
    solid_binary = build_probe(
        solid_directory,
        units=SOLID_UNITS,
        probe_source=ROOT / "test/harness/spatial/HeightSolidProbe.cpp",
    )
    cut_directory = args.output / "height-cuts"
    cut_directory.mkdir()
    cut_binary = build_probe(
        cut_directory,
        units=CUT_UNITS,
        probe_source=ROOT / "test/harness/spatial/HeightBandProbe.cpp",
    )
    reports = []
    baselines = {name: construct(name) for name in ("stacked", "crossed")}
    original_envelope, original_bands = (
        stair_envelope_remesh.coarsen_envelope,
        stair_band_remesh.remesh,
    )

    def envelope(plan, heights):
        result, note = original_envelope(plan, heights)
        faces, masks = unique_caps(plan)
        xyz = np.c_[plan["xy"], heights]
        flights = {flight["owner"]: flight for flight in plan["stair_lofts"]}
        rebuilt, owners, _steps, work = compare(
            binary,
            args.output / f"stage-{len(reports)}",
            xyz,
            faces,
            masks,
            None,
            flights,
        )
        reports.append(dict(stage="envelope", **work))
        for owner in range(len(plan["rows"])):
            selected = (owners & (1 << owner)) != 0
            equivalent(xyz, rebuilt[selected], result["parts"][owner])
            result["parts"][owner] = rebuilt[selected]
            result["part_global_faces"][owner] = np.flatnonzero(selected)
        return result, note

    def bands(points, faces, owners, steps, levels, note, plan):
        reference = original_bands(points, faces, owners, steps, levels, note, plan)
        flights = {flight["owner"]: flight for flight in plan["stair_lofts"]}
        rebuilt, masks, band, work = compare(
            binary,
            args.output / f"stage-{len(reports)}",
            points,
            faces,
            owners,
            steps,
            flights,
        )
        retained, inverse = np.unique(rebuilt, return_inverse=True)
        selected_points, rebuilt = points[retained], inverse.reshape(-1, 3)
        np.testing.assert_array_equal(selected_points, reference[0])
        for owner, step in set(zip(masks, band, strict=True)):
            actual = rebuilt[(masks == owner) & (band == step)]
            expected = reference[1][(reference[2] == owner) & (reference[3] == step)]
            equivalent(selected_points, actual, expected)
        reports.append(dict(stage="bands", **work))
        return selected_points, rebuilt, masks, band, *reference[4:]

    def height_cuts(plan, heights):
        result, work = consume_cuts(
            cut_binary, args.output / f"stage-{len(reports)}", plan, heights
        )
        reports.append(dict(stage="height_cuts", **work))
        return result

    def solid(points, faces, steps, levels):
        directory = args.output / f"stage-{len(reports)}"
        directory.mkdir()
        source = directory / "input.npz"
        np.savez_compressed(
            source, points=points, faces=faces, step=steps, levels=levels
        )
        report = compare_solid(solid_binary, source, directory / "closed")
        actual, metadata, count = read_solid(directory / "closed.mesh")
        reports.append(dict(stage="closed_solid", **report))
        return (
            actual,
            actual.triangles[:count],
            actual.triangles[count : 2 * count, ::-1],
            actual.triangles[2 * count :],
            metadata[:, 1],
            report["volume_m3"],
        )

    with (
        patch.object(stair_envelope_remesh, "coarsen_envelope", side_effect=envelope),
        patch.object(stair_band_remesh, "remesh", side_effect=bands),
        patch.object(stair_junction_body, "close", side_effect=solid),
        patch.object(stair_indexed_levels, "partition", side_effect=height_cuts),
    ):
        for name in ("stacked", "crossed"):
            result = construct(name)
            if len(result["body"].triangles) != len(baselines[name]["body"].triangles):
                raise ValueError(
                    "native regions changed the complete body triangle count"
                )
            directory = args.output / name
            directory.mkdir()
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
                    "body_faces": len(result["body"].triangles),
                    "float32_volumes_m3": result["float32_body_volumes_m3"],
                    "native_faces_consumed": True,
                    "indexed_boundaries_equal": True,
                }
            )
    (args.output / "result.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    run(parser.parse_args())
