"""Compare native source height identities against the unchanged prepared Python layout."""

import argparse
import json
import shlex
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
import stair_junction_layout as layout
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from native_planar_arrangement import read_output
from native_planar_mesh import ROOT, build_probe
from planar_layout_reference import original_layout
from stair_test_transform import rigid_coordinates


def write_ports(path, masks, incident, transform, domains):
    with path.open("w") as stream:
        stream.write(f"{domains} {len(masks)} 1e-8\n")
        for mask, owners in zip(masks, incident, strict=True):
            stream.write(f"{len(owners)} " + " ".join(map(str, owners)) + "\n")
            stream.write(f"{1 + len(mask.interiors)}\n")
            for ring in [mask.exterior, *mask.interiors]:
                points = transform(np.asarray(ring.coords))
                stream.write(f"{len(points)}\n")
                for point in points:
                    stream.write(" ".join(format(x, ".17g") for x in point) + "\n")


def read_topology(path, expected, planar_xy):
    lines = iter(path.read_text().splitlines())
    vertices, sources = map(int, next(lines).split())
    if vertices != len(expected["xy"]) or sources != len(expected["parts"]):
        raise ValueError("native height variable/source count differs")
    records = np.array([list(map(float, next(lines).split())) for _ in range(vertices)])
    if not np.array_equal(
        records[:, :2].copy().view(np.uint64), expected["xy"].view(np.uint64)
    ):
        raise ValueError("native source-specific height coordinates differ")
    if not np.array_equal(records[:, 2], expected["primary_source"]):
        raise ValueError("native primary height source differs")
    lookup = {tuple(point): at for at, point in enumerate(planar_xy)}
    if not np.array_equal(
        records[:, 3], [lookup[tuple(point)] for point in expected["xy"]]
    ):
        raise ValueError(
            "native height variables lost their original planar vertex identity"
        )
    for faces, global_faces in zip(
        expected["parts"], expected["part_global_faces"], strict=True
    ):
        count = int(next(lines))
        data = np.array([list(map(int, next(lines).split())) for _ in range(count)])
        if not np.array_equal(data[:, :3], faces) or not np.array_equal(
            data[:, 3], global_faces
        ):
            raise ValueError(
                "native height/source faces or original planar face identities differ"
            )
    if next(lines, None) is not None:
        raise ValueError("unexpected trailing native topology output")


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    native = args.output / "native"
    native.mkdir()
    units = (
        "SurfaceHeightTopology.cpp",
        "SurfaceHeightPorts.cpp",
        "GeosPolygon.cpp",
        "PlanHierarchy.cpp",
    )
    libraries = shlex.split(
        subprocess.check_output(["pkg-config", "--libs", "geos"], text=True)
    )
    binary = build_probe(
        native,
        units=units,
        probe_source=ROOT
        / "test/experiments/transport/NativeSurfaceHeightTopologyProbe.cpp",
        libraries=libraries,
    )
    _reference, _raw, (rows, domains, masks, _kwargs) = original_layout()
    _case, sources = inputs()
    proposal = plan_connections(sources)
    owners = {row["source_id"]: i for i, row in enumerate(rows)}
    incident = [
        [owners[c["source_id"]], owners[c["target_source_id"]]]
        for c in proposal["connections"]
    ]
    nodes = [c["xy_m"] for c in proposal["connections"]]
    results = []
    controls = (
        ("original", 0, [0, 0]),
        ("rotate", 0.37, [0, 0]),
        ("shift", 0, [1024, -2048]),
        ("rotate-shift", -0.83, [100000, -120000]),
    )
    for name, angle, delta in controls:
        transform, _rotation = rigid_coordinates(angle, np.asarray(delta))
        mesh = args.arrangement / (name + "-native.txt")
        xy, faces, face_owners = read_output(mesh, len(domains))
        moved_masks = [
            shapely.Polygon(
                transform(np.asarray(m.exterior.coords)),
                [transform(np.asarray(r.coords)) for r in m.interiors],
            )
            for m in masks
        ]
        with (
            patch.object(
                layout,
                "landings",
                return_value=(transform(nodes), incident, moved_masks),
            ),
            patch.object(
                layout, "arrangement", return_value=(xy, faces, face_owners, {})
            ),
        ):
            reference = layout.layout(rows, "width", source_domains=domains)
        source, output = (
            args.output / (name + "-ports.txt"),
            args.output / (name + "-native.txt"),
        )
        write_ports(source, masks, incident, transform, len(domains))
        command = [str(binary), str(mesh), str(source), str(output)]
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
        read_topology(output, reference, xy)
        record.update(
            variables=len(reference["xy"]),
            sources=len(reference["parts"]),
            source_faces=sum(map(len, reference["parts"])),
            metrics=json.loads(result.stdout),
            exact_reference=True,
        )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "prepared_native_arrangement": str(args.arrangement),
        "runs": results,
        "scope": "Actual prepared native arrangement, separate source height variables, ordered qualified port unions and source/global face lineage. Python layout algorithm unchanged; only its prepared geometry/landing inputs replaced at the operator boundary. No height LP assembly, station loft, outer profile binding or world/GPU admission.",
    }
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arrangement", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
