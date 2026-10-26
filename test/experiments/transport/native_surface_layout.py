"""Compare native landing/section planning and arrangement on actual source domains (#1189)."""

import argparse
import copy
import json
import subprocess
from collections import defaultdict
from pathlib import Path

import numpy as np
import shapely
from mvt_native_cut_domains import cut_ribbon
from mvt_native_cut_profiles import cut_profiles
from mvt_native_path_sections import sections
from mvt_stair_native_crosscuts import FIXTURE
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from native_planar_arrangement import validate, validate_native_profiles
from native_planar_mesh import ROOT, UNITS, build_probe
from shared_bridge_geometry import corridor
from stair_junction_layout import landings

NATIVE_UNITS = (
    "SurfaceLayout.cpp",
    *UNITS,
    "PlanarArrangement.cpp",
    "PlanarArrangementCoverage.cpp",
    "GeosPolygon.cpp",
    "SurfaceLandings.cpp",
    "SurfaceSections.cpp",
    "SurfaceLayoutConstraints.cpp",
)


def source_inputs():
    case, sources = inputs()
    proposal = plan_connections(sources)
    grouped = defaultdict(list)
    for profile in cut_profiles(json.loads(FIXTURE.read_text())):
        grouped[profile["source_id"]].append(profile)
    rows, domains = [], []
    for way, source in zip(case["ways"], sources, strict=True):
        ports = sorted(
            grouped[source["source_id"]], key=lambda p: p["source_station_m"]
        )
        if ports:
            axis, domain, _work = cut_ribbon(source["xy_m"], *ports)
            width = max(p["raw_section"]["width_m"] for p in ports)
        else:
            axis = np.asarray(proposal["axes_xy_m"][source["source_id"]])
            direction = axis[-1] - axis[0]
            tangent = direction / np.linalg.norm(direction)
            if abs((axis - axis[0]) @ np.array([-tangent[1], tangent[0]])).max() > 1e-8:
                raise ValueError("existing straight-flight recipe required")
            axis = axis[[0, -1]]
            width = 2 * source["half_width_m"]
            domain = corridor({"xy": axis, "width_m": width})
        rows.append(
            {
                "xy": axis,
                "width_m": width,
                "source_id": source["source_id"],
                "original": {"source": {"feature": {"properties": way["tags"]}}},
            }
        )
        domains.append(domain)
    owners = {row["source_id"]: index for index, row in enumerate(rows)}
    nodes = [
        {
            "xy": c["xy_m"],
            "incident": [owners[c["source_id"]], owners[c["target_source_id"]]],
        }
        for c in proposal["connections"]
    ]
    return rows, domains, nodes


def transformed(rows, domains, nodes, angle, delta):
    matrix = np.array([[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]])
    cache = {}

    def move(points):
        result = []
        for point in np.asarray(points):
            key = tuple(point)
            if key not in cache:
                cache[key] = np.asarray(point) @ matrix.T + delta
            result.append(cache[key])
        return np.asarray(result)

    moved = copy.deepcopy(rows)
    for row in moved:
        row["xy"] = move(row["xy"])
    polygons = [
        shapely.Polygon(move(p.exterior.coords), [move(r.coords) for r in p.interiors])
        for p in domains
    ]
    junctions = [dict(node, xy=move([node["xy"]])[0]) for node in nodes]
    return moved, polygons, junctions, move


def write_input(path, rows, domains, nodes):
    def line(stream, points):
        stream.write(str(len(points)) + "\n")
        for point in points:
            stream.write(" ".join(format(float(v), ".17g") for v in point) + "\n")

    with path.open("w") as stream:
        stream.write(f"{len(rows)} {len(nodes)} 0 2\n")
        for row, domain in zip(rows, domains, strict=True):
            steps = (
                row["original"]["source"]["feature"]["properties"]["subclass"]
                == "steps"
            )
            stream.write(
                f"{row['width_m']:.17g} {int(steps)} {1 + len(domain.interiors)}\n"
            )
            line(stream, row["xy"])
            for ring in (domain.exterior, *domain.interiors):
                line(stream, ring.coords)
        for node in nodes:
            stream.write(
                " ".join(format(float(v), ".17g") for v in node["xy"])
                + " "
                + str(len(node["incident"]))
                + " "
                + " ".join(map(str, node["incident"]))
                + "\n"
            )


def read_output(path, sources):
    rows = iter(path.read_text().splitlines())

    def line():
        return np.array(
            [list(map(float, next(rows).split())) for _ in range(int(next(rows)))]
        )

    ports, count, source_count, points, faces = map(int, next(rows).split())
    if source_count != sources:
        raise ValueError("one section range per source required")
    regions, incident = [], []
    for _ in range(ports):
        owners, rings = map(int, next(rows).split())
        ids = list(map(int, next(rows).split()))
        if owners != len(ids):
            raise ValueError("native landing ownership count differs")
        contours = [line() for _ in range(rings)]
        regions.append(shapely.Polygon(contours[0], contours[1:]))
        incident.append(ids)
    ranges = [list(map(int, next(rows).split())) for _ in range(sources)]
    constraints = [line() for _ in range(count)]
    xy = np.array([list(map(float, next(rows).split())) for _ in range(points)])
    triangles = np.array(
        [list(map(int, next(rows).split())) for _ in range(faces)], dtype=np.uint64
    )
    work = dict(
        zip(
            (
                "input_coordinates",
                "landing_pairs",
                "section_queries",
                "section_parts",
                "output_coordinates",
                "copied_domain_points",
                "constraint_queries",
                "constraint_edge_tests",
            ),
            map(int, next(rows).split()),
            strict=True,
        )
    )
    if next(rows, None) is not None:
        raise ValueError("trailing native output")
    return regions, incident, ranges, constraints, xy, triangles, work


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=NATIVE_UNITS,
        probe_source=ROOT / "test/harness/spatial/SurfaceLayoutProbe.cpp",
        libraries=subprocess.check_output(
            ["pkg-config", "--libs", "geos"], text=True
        ).split(),
    )
    original = source_inputs()
    results = []
    for name, angle, delta in [
        ("original", 0, [0, 0]),
        ("translate", 0, [800, -300]),
        ("rotate", 0.37, [0, 0]),
    ]:
        rows, domains, nodes, move = transformed(*original, angle, delta)
        source, output = (
            args.output / (name + ".input"),
            args.output / (name + ".output"),
        )
        write_input(source, rows, domains, nodes)
        subprocess.run([str(binary), str(source), str(output)], check=True)
        regions, incident, ranges, constraints, xy, triangles, work = read_output(
            output, len(rows)
        )
        _nodes, expected_incident, masks = landings(
            rows, domains, "width", 0, junctions=nodes
        )
        if incident != expected_incident:
            raise ValueError("native landing changed declared source order")
        for native, expected in zip(regions, masks, strict=True):
            if (
                native.symmetric_difference(expected).area > 1e-7
                or native.hausdorff_distance(expected) > 1e-12
            ):
                raise ValueError(
                    "native landing differs from the existing width/overlap recipe"
                )
        expected_constraints = [
            (np.asarray(r.coords), None)
            for m in masks
            for r in (m.exterior, *m.interiors)
        ]
        for owner, (row, domain, (first, count)) in enumerate(
            zip(rows, domains, ranges, strict=True)
        ):
            expected = [
                np.asarray(part.coords)
                for geometry in sections(row, domain, 2)
                for part in shapely.get_parts(geometry)
            ]
            if len(expected) != count:
                raise ValueError("native source section count differs")
            for native, reference in zip(
                constraints[first : first + count], expected, strict=True
            ):
                np.testing.assert_allclose(native, reference, atol=1e-12, rtol=0)
            expected_constraints.extend((points, owner) for points in expected)
        owners = (
            triangles[:, 3, None]
            & (np.uint64(1) << np.arange(len(rows), dtype=np.uint64))
        ) != 0
        errors = validate(xy, triangles[:, :3], owners, domains, expected_constraints)
        profile_work = validate_native_profiles(
            xy, triangles[:, :3], owners, rows, move
        )
        results.append(
            {
                "control": name,
                "points": len(xy),
                "faces": len(triangles),
                "work": work,
                "maximum_domain_area_error_m2": max(errors),
                **profile_work,
            }
        )
    result = {
        "numpy": np.__version__,
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "cases": results,
        "scope": "Declared source domains/axes/incidence to native landing masks, clipped geometry sections and conforming arrangement. Source construction, height ownership/flights and runtime admission are separate consumers.",
    }
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
