"""Compare selected canonical face cuts with Python and full profile boundaries (#1189)."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from native_planar_arrangement import read_output
from native_planar_mesh import ROOT, build_probe
from native_station_mesh import check_cut, compare_reference, read_stations

FIXTURE = Path(__file__).with_name("data") / "station-profile-boundaries.json"
UNITS = (
    "PlanarStationMesh.cpp",
    "PlanarStationCuts.cpp",
    "PlanarStationConformance.cpp",
)


def transformed(data, angle, translation):
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )
    cache = {}

    def move(points):
        result = []
        for point in points:
            key = tuple(point)
            if key not in cache:
                cache[key] = np.array(point) @ rotation.T + translation
            result.append(cache[key])
        return np.array(result)

    return (
        move(data["xy_m"]),
        [
            dict(
                axis,
                origin_xy_m=move([axis["origin_xy_m"]])[0],
                tangent=np.array(axis["tangent"]) @ rotation.T,
            )
            for axis in data["axes"]
        ],
        [dict(profile, xy_m=move(profile["xy_m"])) for profile in data["profiles"]],
    )


def write_inputs(directory, xy, faces, masks, axes):
    mesh, frames, selection = (
        directory / name for name in ("mesh", "axes", "selection")
    )
    with mesh.open("w") as stream:
        stream.write(f"{len(xy)} {len(faces)}\n")
        for point in xy:
            stream.write(" ".join(format(v, ".17g") for v in point) + "\n")
        for face, mask in zip(faces, masks, strict=True):
            stream.write(" ".join(map(str, [*face, mask])) + "\n")
    with frames.open("w") as stream:
        stream.write(f"{len(axes)}\n")
        for axis in axes:
            values = [
                *axis["origin_xy_m"],
                *axis["tangent"],
                len(axis["cuts_m"]),
                *axis["cuts_m"],
            ]
            stream.write(" ".join(format(v, ".17g") for v in values) + "\n")
    with selection.open("w") as stream:
        stream.write(f"{len(axes)}\n")
        for axis in axes:
            values = [len(axis["initial_faces"]), *axis["initial_faces"]]
            stream.write(" ".join(map(str, values)) + "\n")
    return mesh, frames, selection


def compare(binary, directory, data, xy, axes, profiles, scope):
    directory.mkdir()
    faces, masks = np.array(data["triangles"]), np.array(data["source_masks"])
    count = max(int(v).bit_length() for v in masks)
    owners = (masks[:, None] & (1 << np.arange(count))) != 0
    before = xy, faces, owners
    mesh, frames, selection = write_inputs(directory, xy, faces, masks, axes)
    command = [
        str(binary),
        str(mesh),
        str(frames),
        str(directory / "result"),
        str(directory / "stations"),
    ]
    if scope == "local":
        command.append(str(selection))
    process = subprocess.run(command, capture_output=True, text=True, check=False)
    (directory / "process.json").write_text(
        json.dumps(
            {
                "command": command,
                "returncode": process.returncode,
                "stdout": process.stdout,
                "stderr": process.stderr,
            },
            indent=2,
        )
        + "\n"
    )
    process.check_returncode()
    after = read_output(directory / "result", count)
    stations, parents = read_stations(
        directory / "stations", len(axes), len(after[0]), len(after[1])
    )
    selections = [axis["initial_faces"] for axis in axes] if scope == "local" else None
    comparison = compare_reference(
        before, after, frames, stations, parents, selections=selections
    )
    domains = [
        shapely.union_all(shapely.polygons(xy[faces[owners[:, i]]]))
        for i in range(count)
    ]
    flights = [
        {"start_cut_m": axis["cuts_m"][0], "end_cut_m": axis["cuts_m"][-1]}
        for axis in axes
    ]
    errors = check_cut(
        before, after, stations, parents, domains, flights, selections=selections
    )
    coverage = []
    for profile in profiles:
        cells = shapely.polygons(after[0][after[1][after[2][:, profile["owner"]]]])
        coverage.append(
            bool(shapely.union_all(cells).covers(shapely.LineString(profile["xy_m"])))
        )
    if scope == "local" and not all(coverage):
        raise ValueError("local canonical cuts changed a fixed whole profile boundary")
    return {
        "scope": scope,
        "points": len(after[0]),
        "faces": len(after[1]),
        "work": json.loads(process.stdout),
        "reference": comparison,
        "full_profile_coverage": coverage,
        "maximum_source_area_error_m2": max(errors),
    }


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=UNITS,
        probe_source=ROOT / "test/harness/spatial/PlanarStationProbe.cpp",
    )
    data = json.loads(args.input.read_text())
    results = []
    for name, angle, delta in [
        ("original", 0, [0, 0]),
        ("translate", 0, [800, -300]),
        ("rotate", 0.37, [0, 0]),
    ]:
        xy, axes, profiles = transformed(data, angle, delta)
        for scope in ("global", "local"):
            result = compare(
                binary,
                args.output / (name + "-" + scope),
                data,
                xy,
                axes,
                profiles,
                scope,
            )
            results.append(dict(control=name, **result))
    result = {
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "input": str(args.input),
        "cases": results,
        "scope": "Selected canonical faces and their actual edge neighbors; "
        "global is a diagnostic comparison. No height/body/world admission.",
    }
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
