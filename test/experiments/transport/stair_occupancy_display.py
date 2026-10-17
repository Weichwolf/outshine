"""Whole private 3D diagnostics of actual source footprints; no building architecture."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from cycle_bridge_display import append
from foot_ramp_geometry import close_caps
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Mesh
from stair_module_inputs import read_arrays
from stair_occupancy_inputs import actual_sources, buildings, full_tile_sampler
from stair_terrain_experiment import frozen
from surface_solid_checks import audit_closed


def footprint_body(footprint):
    raw = shapely.get_coordinates(
        shapely.constrained_delaunay_triangles(footprint.geometry)
    ).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    faces = inverse.reshape(-1, 3)
    area = np.cross(
        np.c_[xy[faces[:, 1]] - xy[faces[:, 0]], np.zeros(len(faces))],
        np.c_[xy[faces[:, 2]] - xy[faces[:, 0]], np.zeros(len(faces))],
    )[:, 2]
    faces[area < 0] = faces[area < 0, ::-1]
    actual = shapely.union_all(shapely.polygons(xy[faces]))
    if actual.symmetric_difference(footprint.geometry).area > max(
        1e-8, footprint.geometry.area * 1e-10
    ):
        raise ValueError("diagnostic tessellation changed original footprint")
    upper = Mesh(np.c_[xy, np.full(len(xy), footprint.heights.upper_m)], faces)
    lower = Mesh(np.c_[xy, np.full(len(xy), footprint.heights.lower_m)], faces)
    body = close_caps(upper, lower)
    if audit_closed(body) <= 0:
        raise ValueError("diagnostic footprint occupancy is not a valid closed solid")
    return body


def run(args):
    commit = frozen()
    root = args.products
    if not root.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private occupancy diagnostics must live under /tmp")
    report = json.loads((root / "comparison.json").read_text())
    verified = json.loads((root / "verification.json").read_text())
    if not verified["valid_saved_occupancy"] or verified[
        "products_report_sha256"
    ] != digest(root / "comparison.json"):
        raise ValueError("verified saved occupancy products required")
    context, _sources, _sample, _layer = actual_sources()
    originals, _ledger, _receipt = buildings(
        context, full_tile_sampler(Path(report["dem"]["path"]), context)
    )
    by_key = {p.key: p for p in originals}
    adjusted = []
    for row in json.loads((root / "adjusted-footprints.json").read_text()):
        original = by_key[(row["source_id"], tuple(row["part"]))]
        adjusted.append(
            BuildingFootprint(
                row["source_id"],
                shapely.from_wkb(row["geometry_wkb"]),
                HeightInterval(*row["heights_m"]),
                tuple(row["part"]),
                original.attributes,
            )
        )
    output = root / "display"
    output.mkdir(exist_ok=False)
    provenance = {
        "commit": commit,
        "products_report_sha256": digest(root / "comparison.json"),
        "scope": "Actual provider footprint height prisms only, no architecture recipes; whole 720p diagnostic.",
    }
    for scene_path in args.scene:
        scene = json.loads(scene_path.read_text())
        if scene["profile"] != [1280, 720] or scene.get("detail_view"):
            raise ValueError("whole 1280x720 reference scenes required")
        mesh = Path(scene["mesh"])
        if digest(mesh) != scene["mesh_sha256"]:
            raise ValueError("original complete 65-source display changed")
        for phase, footprints in (("before", originals), ("after", adjusted)):
            name = phase + "-" + scene_path.stem.rsplit("-", 2)[-2]
            path = output / (name + ".npz")
            palette = np.array([*scene["colours"], [0.52, 0.46, 0.38, 1]])
            shown = append(
                read_arrays(mesh),
                [(footprint_body(p), len(palette) - 1) for p in footprints],
                palette,
            )
            np.savez_compressed(path, **shown)
            row = dict(
                scene,
                id="Wien-affine-occupancy-" + name,
                colours=palette.tolist(),
                mesh=str(path.resolve()),
                mesh_sha256=digest(path),
                output=str((output / (name + ".png")).resolve()),
            )
            (output / (name + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
            provenance[name] = {
                "reference_scene": str(scene_path.resolve()),
                "reference_scene_sha256": digest(scene_path),
                "mesh_sha256": digest(path),
                "footprint_count": len(footprints),
            }
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--scene", type=Path, action="append", required=True)
    run(parser.parse_args())
