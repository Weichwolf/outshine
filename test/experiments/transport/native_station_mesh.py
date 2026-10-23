"""Cut the actual native planar body before source heights, then consume its profiles."""

import argparse
import copy
import hashlib
import json
import shlex
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
import stair_junction_layout as layout
from mvt_native_body_layout import prepare_native_body_plan
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_stair_loft import _flight
from native_planar_arrangement import read_output, validate_native_profiles
from native_planar_mesh import ROOT, build_probe
from native_profile_fit import body
from native_surface_height_topology import read_topology, write_ports
from planar_layout_reference import hanging_pairs, original_layout
from stair_station_cuts import _split
from stair_test_transform import rigid_coordinates


def source_plan(mesh=None, loft=None):
    case, sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    with patch("mvt_t_profile_layout.prepare_stair_lofts", side_effect=loft):
        if mesh is None:
            return prepare_native_body_plan(
                case, sources, plan_connections(sources), native
            )
        with patch.object(layout, "arrangement", return_value=mesh):
            return prepare_native_body_plan(
                case, sources, plan_connections(sources), native
            )


def write_axes(path, flights, transform, rotation):
    with path.open("w") as stream:
        stream.write(f"{len(flights)}\n")
        for flight in flights:
            origin = transform([flight["origin_xy_m"]])[0]
            direction = np.asarray(flight["tangent"]) @ rotation.T
            values = [
                *origin,
                *direction,
                2,
                flight["start_cut_m"],
                flight["end_cut_m"],
            ]
            stream.write(" ".join(format(float(x), ".17g") for x in values) + "\n")


def read_stations(path, axes, points, faces):
    lines = path.read_text().splitlines()
    if list(map(int, lines[0].split())) != [axes, points]:
        raise ValueError("complete axis/point station table required")
    stations = np.array([list(map(float, row.split())) for row in lines[1 : axes + 1]])
    parents = np.array(list(map(int, lines[axes + 1].split())), dtype=np.int64)
    if stations.shape != (axes, points) or parents.shape != (faces,):
        raise ValueError("complete station and face-parent output required")
    return stations, parents


def check_cut(before, after, stations, parents, domains, flights):
    old_xy, old_faces, old_owners = before
    xy, faces, owners = after
    if not np.array_equal(xy[: len(old_xy)].view(np.uint64), old_xy.view(np.uint64)):
        raise ValueError("station refinement moved an immutable input coordinate")
    if np.any(parents < 0) or np.any(parents >= len(old_faces)):
        raise ValueError("station output lost original planar face parents")
    if not np.array_equal(owners, old_owners[parents]):
        raise ValueError("station subdivision changed original source ownership")
    if not shapely.coverage_is_valid(shapely.polygons(xy[faces])):
        raise ValueError("native station cuts created mismatched canonical edges")
    errors = []
    for owner, domain in enumerate(domains):
        cells = shapely.polygons(xy[faces[owners[:, owner]]])
        error = float(shapely.union_all(cells).symmetric_difference(domain).area)
        if not shapely.coverage_is_valid(cells) or error > 1e-7:
            raise ValueError(
                "native station cuts changed the existing source footprint"
            )
        errors.append(error)
    for axis, flight in enumerate(flights):
        values = stations[axis][faces]
        for cut in [flight["start_cut_m"], flight["end_cut_m"]]:
            if np.any((values.min(axis=1) < cut) & (values.max(axis=1) > cut)):
                raise ValueError("a native facet crosses its declared station cut")
    return errors


def compare_reference(before, after, axes_path, stations, parents):
    xy, faces, _owners = before
    rows = [
        list(map(float, row.split())) for row in axes_path.read_text().splitlines()[1:]
    ]
    flights = []
    for row, actual in zip(rows, stations, strict=True):
        origin, direction = np.array(row[:2]), np.array(row[2:4])
        delta = xy - origin
        values = delta[:, 0] * direction[0] + delta[:, 1] * direction[1]
        if not np.array_equal(
            values.view(np.uint64), actual[: len(xy)].view(np.uint64)
        ):
            raise ValueError(
                "native station projection differs from independent input axes"
            )
        flights.append(
            {
                "origin_xy_m": origin,
                "tangent": direction,
                "stations_m": values.tolist(),
            }
        )
    vertices, triangles, lineage = (
        list(xy.copy()),
        faces.tolist(),
        list(range(len(faces))),
    )
    masks, planar = lineage.copy(), list(range(len(xy)))
    for axis, row in enumerate(rows):
        for cut in row[5:]:
            triangles, masks, lineage, _intersections, _points = _split(
                vertices,
                flights,
                triangles,
                masks,
                lineage,
                planar,
                axis,
                cut,
                4096,
                8192,
                None,
            )
    same = {
        "position_words_equal": np.array_equal(
            np.array(vertices).view(np.uint64), after[0].view(np.uint64)
        ),
        "faces_equal": np.array_equal(triangles, after[1]),
        "parents_equal": np.array_equal(lineage, parents),
        "station_words_equal": np.array_equal(
            np.array([f["stations_m"] for f in flights]).view(np.uint64),
            stations.view(np.uint64),
        ),
    }
    if not all(same.values()):
        raise ValueError(
            f"native station operator differs from unchanged Python cut: {same}"
        )
    same["proximity_diagnostic_pairs"] = len(hanging_pairs(after[0], after[1]))
    return same


def native_call(command, output):
    result = subprocess.run(
        list(map(str, command)), text=True, capture_output=True, check=False
    )
    receipt = {
        "command": list(map(str, command)),
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }
    output.write_text(json.dumps(receipt, indent=2) + "\n")
    result.check_returncode()
    return json.loads(result.stdout)


def consume_original(mesh, stations, flights, topology, output):
    from mvt_t_profile_solve import solve_profiles

    xy, faces, owners = mesh
    records = topology.read_text().splitlines()
    count = int(records[0].split()[0])
    planar = np.array([int(row.split()[3]) for row in records[1 : count + 1]])

    def prepared_lofts(plan):
        plan["stair_lofts"] = copy.deepcopy(flights)
        for axis, flight in enumerate(plan["stair_lofts"]):
            flight["stations_m"] = stations[axis][planar].tolist()
        plan["station_refinement"] = {
            "producer": "native shared planar cuts before heights"
        }

    plan, contacts = source_plan((xy, faces, owners, {}), prepared_lofts)
    read_topology(topology, plan, xy)
    heights, fit = solve_profiles(
        plan,
        fixed_contacts=contacts,
        max_prior_change_m=3,
        gradient_model="axis_crossfall",
    )
    if heights is None:
        raise ValueError(f"native planar station consumer is infeasible: {fit}")
    data, audits = body(plan, heights, contacts)
    np.savez_compressed(
        output.with_suffix(".npz"),
        **{
            key: data[key]
            for key in (
                "body_vertices",
                "body_faces",
                "top_faces",
                "wall_faces",
                "face_sources",
                "reference_vertices",
                "reference_faces",
            )
        },
    )
    from mvt_native_body_display import display

    display(plan, data, output.parent)
    controls = []
    for model, cap in (("whole_norm", 3), ("axis_crossfall", 2)):
        values, note = solve_profiles(
            plan, fixed_contacts=contacts, max_prior_change_m=cap, gradient_model=model
        )
        if values is not None:
            raise ValueError(
                "a previously infeasible original profile control became feasible"
            )
        controls.append(note)
    result = {
        "fit": fit,
        "audits": audits,
        "negative_controls": controls,
        "sources": len(plan["rows"]),
        "height_variables": count,
        "source_faces": sum(map(len, plan["parts"])),
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    return result


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    build = args.output / "native"
    build.mkdir()
    cut_probe = build_probe(
        build,
        units=("PlanarStationMesh.cpp", "PlanarStationCuts.cpp"),
        probe_source=ROOT / "test/harness/spatial/PlanarStationProbe.cpp",
    )
    base, _ = source_plan()
    flights = [_flight(base, owner) for owner, _low, _high in base["stairs"]]
    _reference, _raw, (rows, domains, masks, _kwargs) = original_layout(
        native_body=True
    )
    topology_build = args.output / "topology-native"
    topology_build.mkdir()
    topology_probe = build_probe(
        topology_build,
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
    controls = [
        ("original", 0, [0, 0]),
        ("rotate", 0.37, [0, 0]),
        ("shift", 0, [1024, -2048]),
        ("rotate-shift", -0.83, [100000, -120000]),
    ]
    results = []
    for name, angle, delta in controls:
        transform, rotation = rigid_coordinates(angle, np.asarray(delta))
        source = args.arrangement / (name + "-native.txt")
        before = read_output(source, len(domains))
        axes, target, station_file = [
            args.output / (name + suffix)
            for suffix in ("-axes.txt", "-mesh.txt", "-stations.txt")
        ]
        write_axes(axes, flights, transform, rotation)
        metrics = native_call(
            [cut_probe, source, axes, target, station_file],
            args.output / (name + "-cut-result.json"),
        )
        after = read_output(target, len(domains))
        stations, parents = read_stations(
            station_file, len(flights), len(after[0]), len(after[1])
        )
        comparison = compare_reference(before, after, axes, stations, parents)
        moved_domains = [
            shapely.Polygon(
                transform(np.asarray(d.exterior.coords)),
                [transform(np.asarray(r.coords)) for r in d.interiors],
            )
            for d in domains
        ]
        errors = check_cut(before, after, stations, parents, moved_domains, flights)
        profiles = validate_native_profiles(*after, rows, transform)
        ports, topology = [
            args.output / (name + suffix) for suffix in ("-ports.txt", "-heights.txt")
        ]
        write_ports(ports, masks, base["incident"], transform, len(domains))
        height_metrics = native_call(
            [topology_probe, target, ports, topology],
            args.output / (name + "-height-result.json"),
        )
        row = dict(
            name=name,
            points=len(after[0]),
            faces=len(after[1]),
            cut_metrics=metrics,
            height_metrics=height_metrics,
            source_area_errors_m2=errors,
            unchanged_python_cut=comparison,
            **profiles,
        )
        if name == "original":
            row["consumer"] = consume_original(
                after,
                stations,
                flights,
                topology,
                args.output / "original-consumer.json",
            )
        results.append(row)
        (args.output / "runs.json").write_text(json.dumps(results, indent=2) + "\n")
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__),
                Path(__file__).with_name("mvt_t_stair_loft.py"),
                Path(__file__).with_name("stair_planar_lineage.py"),
            )
        },
        "runs": results,
        "scope": "Native planar station cuts and source height identities; flight planning, LP assembly, body/navigation/terrain still Python. Actual outer profiles and body audits, no runtime/cache/GPU/world admission.",
    }
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arrangement", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
