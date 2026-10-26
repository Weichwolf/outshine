"""Compare native landing ownership and flight cuts with the prepared Python recipe."""

import argparse
import copy
import json
import shlex
import subprocess
from pathlib import Path

import numpy as np
import shapely
from mvt_t_stair_loft import _flight
from native_planar_arrangement import read_output
from native_planar_mesh import ROOT, build_probe
from native_station_mesh import (
    check_cut,
    compare_reference,
    consume_original,
    source_plan,
)
from native_surface_height_topology import write_ports
from planar_layout_reference import original_layout
from stair_test_transform import rigid_coordinates


def read_height_plan(path):
    lines = iter(path.read_text().splitlines())
    vertices, sources = map(int, next(lines).split())
    records = np.array([list(map(float, next(lines).split())) for _ in range(vertices)])
    parts = []
    for _ in range(sources):
        count = int(next(lines))
        part = np.array([list(map(int, next(lines).split())) for _ in range(count)])
        parts.append(part[:, :3])
    if next(lines, None) is not None:
        raise ValueError("unexpected height topology suffix")
    return records[:, :2].copy(), parts


def declarations(plan):
    rows = []
    for owner, _, _ in plan["stairs"]:
        source = plan["rows"][owner]["source_id"]
        ports = {
            c["source_endpoint"]: i
            for i, c in enumerate(plan["construction_contacts"])
            if c["source_id"] == source
        }
        rows.append((owner, ports[0], ports[-1], plan["stair_directions"][owner]))
    return rows


def write_declarations(path, plan, endpoints, transform):
    with path.open("w") as stream:
        stream.write(f"{len(plan['rows'])} {len(endpoints)}\n")
        stairs = {row[0] for row in endpoints}
        for owner, row in enumerate(plan["rows"]):
            points = transform(row["xy"])
            stream.write(f"{int(owner in stairs)} {len(points)}\n")
            for point in points:
                stream.write(" ".join(format(x, ".17g") for x in point) + "\n")
        for values in endpoints:
            stream.write(" ".join(map(str, values)) + "\n")


def moved_mask(mask, transform):
    return shapely.Polygon(
        transform(np.asarray(mask.exterior.coords)),
        [transform(np.asarray(r.coords)) for r in mask.interiors],
    )


def check_native(plan, endpoints, topology, members, flights, transform):
    reference = copy.deepcopy(plan)
    reference["xy"], reference["parts"] = read_height_plan(topology)
    for row in reference["rows"]:
        row["xy"] = np.asarray(transform(row["xy"]))
    reference["masks"] = [moved_mask(mask, transform) for mask in plan["masks"]]
    ports = members.read_text().splitlines()
    if int(ports[0]) != len(reference["masks"]):
        raise ValueError("one native membership list per prepared junction required")
    for line, mask, owners in zip(
        ports[1:], reference["masks"], plan["incident"], strict=True
    ):
        vertices = np.unique(
            np.concatenate([reference["parts"][i].ravel() for i in owners])
        )
        expected = vertices[
            shapely.dwithin(mask, shapely.points(reference["xy"][vertices]), 1e-8)
        ]
        actual = np.array(list(map(int, line.split())))
        if actual[0] != len(expected) or not np.array_equal(actual[1:], expected):
            raise ValueError(
                "native qualified junction membership differs from frozen Python patches"
            )
    lines = iter(flights.read_text().splitlines())
    if tuple(map(int, next(lines).split())) != (len(endpoints), len(reference["xy"])):
        raise ValueError("complete native flight/height-variable table required")
    axes = []
    for owner, start, end, direction in endpoints:
        data = np.array(list(map(float, next(lines).split())))
        actual = np.array(list(map(float, next(lines).split())))
        expected = _flight(reference, owner)
        if not np.array_equal(data[:4], [owner, start, end, direction]):
            raise ValueError(
                "native flight lost its declared source or endpoint owners"
            )
        wanted = np.r_[
            expected["start_cut_m"],
            expected["end_cut_m"],
            expected["origin_xy_m"],
            expected["tangent"],
        ]
        if not np.array_equal(data[4:].view(np.uint64), wanted.view(np.uint64)):
            raise ValueError(f"native cut/axis words differ: {data[4:] - wanted}")
        if not np.array_equal(
            actual.view(np.uint64), np.array(expected["stations_m"]).view(np.uint64)
        ):
            raise ValueError(
                f"native station words differ: {abs(actual - expected['stations_m']).max()}"
            )
        axes.append([*data[6:10], 2, *data[4:6]])
    if next(lines, None) is not None:
        raise ValueError("unexpected native flight suffix")
    return axes


def check_preparation(
    binary,
    args,
    name,
    mesh,
    ports,
    definitions,
    initial_axes,
    base,
    endpoints,
    transform,
    domains,
):
    refined, heights, members, flights = [
        args.output / (name + suffix)
        for suffix in (
            "-refined-mesh.txt",
            "-refined-heights.txt",
            "-refined-members.txt",
            "-refined-flights.txt",
        )
    ]
    command = list(
        map(str, [binary, mesh, ports, definitions, refined, heights, members, flights])
    )
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    receipt = {
        "command": command,
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }
    (args.output / (name + "-preparation.json")).write_text(
        json.dumps(receipt, indent=2) + "\n"
    )
    result.check_returncode()
    before, after = [read_output(path, len(domains)) for path in (mesh, refined)]
    lines = heights.read_text().splitlines()
    count = int(lines[0].split()[0])
    metadata = np.array(
        [list(map(float, line.split())) for line in lines[1 : count + 1]]
    )
    planar = metadata[:, 3].astype(np.int64)
    if not np.array_equal(
        metadata[:, :2].copy().view(np.uint64), after[0][planar].copy().view(np.uint64)
    ):
        raise ValueError(
            "refined height variables lost their exact canonical planar positions"
        )
    data = iter(flights.read_text().splitlines())
    if tuple(map(int, next(data).split())) != (len(endpoints), count):
        raise ValueError("complete rebound flight stations required")
    stations, recipes = [], []
    reference_flights = [_flight(base, row[0]) for row in endpoints]
    for endpoint, reference in zip(endpoints, reference_flights, strict=True):
        description = np.array(list(map(float, next(data).split())))
        heights_station = np.array(list(map(float, next(data).split())))
        canonical = np.full(len(after[0]), np.nan)
        canonical[planar] = heights_station
        if not np.isfinite(canonical).all() or not np.array_equal(
            canonical[planar].view(np.uint64), heights_station.view(np.uint64)
        ):
            raise ValueError(
                "one canonical point has missing or inconsistent source station values"
            )
        stations.append(canonical)
        recipe = copy.deepcopy(reference)
        recipe.update(
            start_cut_m=float(description[4]), end_cut_m=float(description[5])
        )
        recipes.append(recipe)
    stations = np.array(stations)
    parents = np.loadtxt(str(refined) + ".parents", dtype=np.int64, ndmin=1)
    selections = [np.flatnonzero(before[2][:, row[0]]).tolist() for row in endpoints]
    comparison = compare_reference(
        before, after, initial_axes, stations, parents, selections=selections
    )
    errors = check_cut(
        before,
        after,
        stations,
        parents,
        [moved_mask(d, transform) for d in domains],
        recipes,
        selections=selections,
    )
    report = {
        "native": json.loads(result.stdout),
        "comparison": comparison,
        "coverage_errors_m2": errors,
    }
    if name == "original":
        report["physical_consumer"] = consume_original(
            after, stations, recipes, heights, args.output / "native-body-result.json"
        )
    return report


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    build = args.output / "native"
    build.mkdir()
    binary = build_probe(
        build,
        units=(
            "SurfaceHeightTopology.cpp",
            "SurfaceHeightPorts.cpp",
            "SurfaceFlightPlan.cpp",
            "GeosPolygon.cpp",
            "PlanHierarchy.cpp",
        ),
        probe_source=ROOT
        / "test/experiments/transport/NativeSurfaceHeightTopologyProbe.cpp",
        libraries=shlex.split(
            subprocess.check_output(["pkg-config", "--libs", "geos"], text=True)
        ),
    )
    preparation_build = args.output / "preparation-native"
    preparation_build.mkdir()
    preparation = build_probe(
        preparation_build,
        units=(
            "SurfaceFlightPreparation.cpp",
            "SurfaceHeightTopology.cpp",
            "SurfaceHeightPorts.cpp",
            "SurfaceFlightPlan.cpp",
            "PlanarStationMesh.cpp",
            "PlanarStationCuts.cpp",
            "PlanarStationConformance.cpp",
            "GeosPolygon.cpp",
            "PlanHierarchy.cpp",
        ),
        probe_source=ROOT / "test/harness/spatial/SurfaceFlightPreparationProbe.cpp",
        libraries=shlex.split(
            subprocess.check_output(["pkg-config", "--libs", "geos"], text=True)
        ),
    )
    base, _ = source_plan()
    endpoints = declarations(base)
    _, _, (_, domains, masks, _) = original_layout(native_body=True)
    results = []
    for name, angle, offset in (
        ("original", 0, [0, 0]),
        ("rotate", 0.37, [0, 0]),
        ("shift", 0, [1024, -2048]),
        ("rotate-shift", -0.83, [100000, -120000]),
    ):
        transform, _ = rigid_coordinates(angle, np.asarray(offset))
        ports, topology, members, definitions, flights, axes = [
            args.output / (name + suffix)
            for suffix in (
                "-ports.txt",
                "-heights.txt",
                "-members.txt",
                "-definitions.txt",
                "-flights.txt",
                "-axes.txt",
            )
        ]
        write_ports(ports, masks, base["incident"], transform, len(domains))
        write_declarations(definitions, base, endpoints, transform)
        mesh = args.arrangement / (name + "-native.txt")
        command = [
            str(binary),
            str(mesh),
            str(ports),
            str(topology),
            str(members),
            str(definitions),
            str(flights),
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        record = {
            "name": name,
            "command": command,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
        results.append(record)
        (args.output / "runs.json").write_text(json.dumps(results, indent=2) + "\n")
        result.check_returncode()
        native_axes = check_native(
            base, endpoints, topology, members, flights, transform
        )
        axes.write_text(
            str(len(native_axes))
            + "\n"
            + "".join(
                " ".join(format(x, ".17g") for x in row) + "\n" for row in native_axes
            )
        )
        record["qualified_membership_and_all_axis_station_words_equal"] = True
        record["preparation"] = check_preparation(
            preparation,
            args,
            name,
            mesh,
            ports,
            definitions,
            axes,
            base,
            endpoints,
            transform,
            domains,
        )
    (args.output / "result.json").write_text(
        json.dumps(
            {
                "runs": results,
                "scope": "Native qualified ports, straight-flight selection, shared cuts and refined source heights from prepared 2D declarations; original case uses unchanged Python height/body consumer. Native body/runtime remain open.",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            [
                {
                    "name": r["name"],
                    "returncode": r["returncode"],
                    "exact": r["qualified_membership_and_all_axis_station_words_equal"],
                }
                for r in results
            ]
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arrangement", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
