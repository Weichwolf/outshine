"""Replay original source polygons and section constraints through native GEOS."""

import argparse
import itertools
import json
import shlex
import subprocess
from pathlib import Path

import numpy as np
import shapely
from mvt_native_cut_profiles import cut_profiles
from mvt_t_connection_inputs import FIXTURE
from native_planar_mesh import ROOT, UNITS, build_probe
from planar_layout_reference import hanging_pairs, original_layout
from stair_junction_layout import cross_sections
from stair_test_transform import rigid_coordinates


def input_lines(rows, masks, kwargs):
    lines = [(p.boundary, None) for p in masks]
    lines.extend((line, None) for line in kwargs.get("constraints", ()))
    provider = kwargs.get("section_provider")
    for owner, row in enumerate(rows):
        if row.get("surface_axis_constraints", True):
            lines.append(
                (shapely.LineString(row.get("surface_axis_xy_m", row["xy"])), owner)
            )
            if "surface_axis_xy_m" in row:
                lines.append((shapely.LineString(row["xy"]), owner))
        sections = cross_sections(row) if provider is None else provider(row)
        lines.extend((section, owner) for section in sections)
    return [
        (np.asarray(line.coords), owner)
        for shape, owner in lines
        for line in shapely.get_parts(shape)
    ]


def write_input(path, domains, lines, transform):
    def line(stream, points):
        points = np.asarray(transform(np.asarray(points)))
        stream.write(f"{len(points)}\n")
        for point in points:
            stream.write(" ".join(format(x, ".17g") for x in point) + "\n")

    with path.open("w") as stream:
        stream.write(f"{len(domains)} {len(lines)} 1e-8 1e-7\n")
        for domain in domains:
            if domain.geom_type != "Polygon":
                raise ValueError(
                    "this retained source fixture requires Polygon domains"
                )
            stream.write(f"{1 + len(domain.interiors)}\n")
            for ring in [domain.exterior, *domain.interiors]:
                line(stream, ring.coords)
        for points, _owner in lines:
            line(stream, points)


def read_output(path, domain_count):
    lines = path.read_text().splitlines()
    points, faces = map(int, lines[0].split())
    if (
        not 0 < points <= 4096
        or not 0 < faces <= 8192
        or len(lines) != 1 + points + faces
    ):
        raise ValueError("bounded complete native arrangement required")
    xy = np.array([list(map(float, line.split())) for line in lines[1 : 1 + points]])
    records = np.array([list(map(int, line.split())) for line in lines[1 + points :]])
    triangles, bits = records[:, :3], records[:, 3]
    if (
        triangles.min() < 0
        or triangles.max() >= points
        or bits.min() < 1
        or bits.max() >= 2**domain_count
    ):
        raise ValueError("complete valid indices and separate ownership masks required")
    owners = (bits[:, None] & (1 << np.arange(domain_count))) != 0
    return xy, triangles, owners


def canonical_faces(faces, owners):
    rows = np.column_stack((np.sort(faces, axis=1), owners))
    return rows[np.lexsort(rows.T[::-1])]


def segment_covered(a, b, edges, tree, tolerance=1e-8):
    a, b = np.asarray(a), np.asarray(b)
    delta = b - a
    length = np.linalg.norm(delta)
    if length <= tolerance:
        return True
    direction = delta / length
    candidates = tree.query(
        shapely.LineString([a, b]), predicate="dwithin", distance=tolerance
    )
    relative = edges[candidates] - a
    error = abs(relative[:, :, 0] * direction[1] - relative[:, :, 1] * direction[0])
    aligned = error.max(axis=1) <= tolerance
    interval = np.sort(relative[aligned] @ direction, axis=1)
    interval = np.clip(interval, 0, length)
    end = 0.0
    for lo, hi in interval[np.argsort(interval[:, 0])]:
        if lo > end + tolerance:
            return False
        end = max(end, hi)
    return end + tolerance >= length


def validate(xy, faces, owners, domains, constraints):
    shapes = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(shapes) or len(hanging_pairs(xy, faces)):
        raise ValueError("native geometry has overlap or hanging vertices")
    errors = [
        shapely.union_all(shapes[owners[:, at]]).symmetric_difference(domain).area
        for at, domain in enumerate(domains)
    ]
    if max(errors) > 1e-7 or not owners.any(1).all():
        raise ValueError("native domain ownership changed original source footprints")
    edges = np.concatenate((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]))
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    edge_points = xy[edges]
    tree = shapely.STRtree(shapely.linestrings(edge_points))
    union = shapely.union_all(domains)
    for line, owner in constraints:
        occupied = union if owner is None else domains[owner]
        clipped = shapely.LineString(line).intersection(occupied)
        for part in shapely.get_parts(clipped):
            if part.geom_type == "Point":
                continue
            points = np.asarray(part.coords)
            for a, b in itertools.pairwise(points):
                if not segment_covered(a, b, edge_points, tree):
                    raise ValueError(
                        "a retained source section or axis lacks conforming mesh edges"
                    )
    return errors


def validate_native_profiles(xy, faces, owners, rows, transform):
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    profiles = cut_profiles(native)
    by_source = {row["source_id"]: i for i, row in enumerate(rows)}
    points = 0
    for profile in profiles:
        owner = by_source[profile["source_id"]]
        used = np.unique(faces[owners[:, owner]])
        actual = {tuple(point) for point in xy[used]}
        required = transform(np.asarray(profile["profile_xyz_m"])[:, :2])
        if any(tuple(point) not in actual for point in required):
            raise ValueError(
                "an immutable native outer-profile vertex lost exact source ownership"
            )
        points += len(required)
    return {
        "fixed_native_profiles": len(profiles),
        "exact_outer_profile_vertices": points,
    }


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    build = args.output / "native"
    build.mkdir()
    units = (
        "PlanarArrangement.cpp",
        "PlanarArrangementCoverage.cpp",
        "GeosPolygon.cpp",
        *UNITS,
    )
    libraries = shlex.split(
        subprocess.check_output(["pkg-config", "--libs", "geos"], text=True)
    )
    binary = build_probe(
        build,
        units=units,
        probe_source=ROOT / "test/harness/spatial/PlanarArrangementProbe.cpp",
        libraries=libraries,
    )
    reference, _raw, (rows, domains, masks, kwargs) = original_layout(
        native_body=args.native_body
    )
    constraints = input_lines(rows, masks, kwargs)
    np.savez_compressed(
        args.output / "python-reference.npz",
        points=reference[0],
        faces=reference[1],
        owners=reference[2],
    )
    results = []
    controls = (
        ("original", 0, [0, 0]),
        ("rotate", 0.37, [0, 0]),
        ("shift", 0, [1024, -2048]),
        ("rotate-shift", -0.83, [100000, -120000]),
    )
    for name, angle, shift in controls:
        transform, _rotation = rigid_coordinates(angle, np.asarray(shift))
        source, target = (
            args.output / (name + ".txt"),
            args.output / (name + "-native.txt"),
        )
        write_input(source, domains, constraints, transform)
        command = [str(binary), str(source), str(target)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        row = {
            "name": name,
            "command": command,
            "terminal": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
        results.append(row)
        (args.output / "runs.json").write_text(json.dumps(results, indent=2) + "\n")
        result.check_returncode()
        xy, faces, owners = read_output(target, len(domains))
        moved = [
            shapely.Polygon(
                transform(np.asarray(p.exterior.coords)),
                [transform(np.asarray(r.coords)) for r in p.interiors],
            )
            for p in domains
        ]
        row.update(
            points=len(xy),
            faces=len(faces),
            domain_area_errors=validate(
                xy,
                faces,
                owners,
                moved,
                [(transform(line), owner) for line, owner in constraints],
            ),
            metrics=json.loads(result.stdout),
        )
        if args.native_body:
            row.update(validate_native_profiles(xy, faces, owners, rows, transform))
        if name == "original":
            row.update(
                reference_points=len(reference[0]),
                reference_faces=len(reference[1]),
                point_words_equal=xy.shape == reference[0].shape
                and np.array_equal(xy.view(np.uint64), reference[0].view(np.uint64)),
                canonical_triangle_owners_equal=np.array_equal(
                    canonical_faces(faces, owners),
                    canonical_faces(reference[1], reference[2]),
                ),
            )
        (args.output / "runs.json").write_text(json.dumps(results, indent=2) + "\n")
    summary = {
        "native_body": args.native_body,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python_geos": shapely.geos_version_string,
        "native_geos": subprocess.check_output(
            ["pkg-config", "--modversion", "geos"], text=True
        ).strip(),
        "runs": results,
        "scope": "Original source domains/masks/axes/sections through native noding, polygonization, CDT, weld/conform and separate owner assignment. No height program, port ownership union, body/nav/terrain/runtime/GPU admission. GEOS temporary allocation peak and library operation count uninstrumented; budgets check input and returned work, not a linear-time claim.",
    }
    (args.output / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-body", action="store_true")
    run(parser.parse_args())
