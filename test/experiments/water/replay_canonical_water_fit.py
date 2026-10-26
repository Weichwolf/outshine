"""Conditional source closure -> existing physical lake fit -> shared regional cut.

A fixture integration check, not a production source-completeness or river-contact
recipe. The native levels are declared soft geodetic model heights, not a verified
ASL/geoid conversion. Source originals and all face memberships remain available.
"""

import argparse
import dataclasses
import json
from pathlib import Path

import numpy as np
import shapely
from canonical_water_domains import resolve
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_physical_boundary import measured
from compare_physical_river import audit
from native_river_inputs import surface_rings
from outshine_ref.contracts import Mesh
from outshine_ref.places.sources import tile_geography
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import (
    model_height_targets,
    normal_caps,
    tangent_directions,
)
from outshine_ref.water.geodetic import local_points
from physical_boundary import boundary_error, canonical_cut, local_piece, to_world
from river_membership import face_membership
from river_surface import surface_mesh


def recipe(domain, data, indices, *, boundary_spacing_m=20.0):
    z, x, y = domain.owner[:3]
    latitude, longitude = tile_geography(np.array([[0.5, 0.5]]), 1, (z, x, y))
    anchor = np.array([longitude[0], latitude[0]])
    polygons = []
    native_indices = [indices[k] for k in domain.sources]
    for index in native_indices:
        rings = [
            local_points(anchor, r[:, ::-1], 0)[:, :2]
            for r in surface_rings(data, index)
        ]
        polygons.append(shapely.normalize(shapely.Polygon(rings[0], rings[1:])))
    zero, topology = surface_mesh(shapely.union_all(polygons), boundary_spacing_m)
    csr = face_membership(zero.vertices, zero.triangles, polygons)
    if np.any(np.diff(csr.offsets) == 0):
        raise ValueError("original source coverage lost in canonical meshing")
    target = float(np.median(data["surfaces"][native_indices]["level_m"]))
    mesh = Mesh(
        np.c_[
            zero.vertices[:, :2],
            model_height_targets(
                anchor, zero.vertices[:, :2], np.full(len(zero.vertices), target)
            ),
        ],
        zero.triangles,
    )
    count = len(mesh.triangles)
    directions = np.r_[
        np.c_[np.arange(count), np.tile([1, 0, 0, -0.005, 0.005], (count, 1))],
        np.c_[np.arange(count), np.tile([0, 1, 0, -0.005, 0.005], (count, 1))],
    ]
    primary = np.c_[np.arange(count), count + np.arange(count)]
    bounds = [mesh.vertices[:, 2].min() - 20, mesh.vertices[:, 2].max() + 20]
    return (
        mesh,
        anchor,
        directions,
        bounds,
        primary,
        polygons,
        csr,
        topology,
        native_indices,
        target,
    )


class ReadWindow(dict):
    def __init__(self, values):
        super().__init__(values)
        self.requested = set()

    def get(self, key):
        value = super().get(key)
        if value is not None:
            self.requested.add(key)
        return value


def run(path, inventory, output):
    data, snapshots, indices = observed_snapshots(path)
    conditional = {
        t: dataclasses.replace(s, complete=True) for t, s in snapshots.items()
    }
    rows = json.loads(inventory.read_text())["rows"]
    candidates = [
        r
        for r in rows
        if r["state"] == "closed"
        and len(r["sources"]) > 1
        and data["inputs"][int(data["surfaces"][r["native_surface"]]["input"])]["kind"]
        == "lake"
    ]
    if not candidates:
        raise ValueError("no conditional multi-source lake for existing-fit replay")
    chosen = min(candidates, key=lambda r: tuple(r["owner"]))
    options = {
        "epoch": (data["sources"][0]["provider"], data["sources"][0]["revision"]),
        "support": 1 / 64,
        "model_revision": "lake-soft-native-median-grade-0.005-v1",
        "assume_ground": True,
    }
    seed = tuple(chosen["owner"])
    domain = resolve(seed, conditional, **options)
    (
        mesh,
        anchor,
        directions,
        bounds,
        primary,
        polygons,
        csr,
        topology,
        native_indices,
        target,
    ) = recipe(domain, data, indices)
    solved, cost = measured(
        lambda: fit_physical_surface(
            mesh, anchor, directions, up_bounds_m=bounds, primary_rows=primary
        )
    )
    if solved.mesh is None or not solved.report.get("finite_qp_optimal"):
        raise ValueError("existing physical fit did not admit the conditional lake")
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, bounds)
    tangents = tangent_directions(
        caps["up"][directions[:, 0].astype(int)], directions[:, 1:4]
    )
    audit_report, _ = audit(solved.mesh, anchor, directions, bounds, tangents)
    if audit_report["sampled_violating_rows"]:
        raise ValueError("independent physical grade control failed")
    cut = canonical_cut(solved.mesh, float(np.median(mesh.vertices[:, 0])))
    anchors = [anchor + [-0.0005, 0], anchor + [0.0005, 0]]
    regions = [local_piece(cut, side, anchor, a) for side, a in enumerate(anchors)]
    error = boundary_error(
        regions[0][0],
        anchors[0],
        regions[0][2],
        regions[1][0],
        anchors[1],
        regions[1][2],
    )
    if error["endpoint_max_m"] > 1e-8:
        raise ValueError(
            "shared physical cut exceeded existing Float64 frame roundoff allowance"
        )
    replays = []
    for source_key in domain.sources:
        # Restrict the source window to exactly the cells that this job actually used.
        window = ReadWindow(conditional)
        resolved = resolve(source_key, window, **options)
        minimal = {t: conditional[t] for t in sorted(window.requested, reverse=True)}
        again = resolve(source_key, minimal, **options)
        if (again.version, again.geometry.wkb) != (domain.version, domain.geometry.wkb):
            raise ValueError("neighbor seed/minimal source window changed fit inputs")
        if resolved.version != again.version:
            raise ValueError("restricted source window changed product version")
        replay_mesh, replay_anchor, replay_rows, replay_bounds, replay_primary, *_ = (
            recipe(again, data, indices)
        )
        for actual, expected in (
            (replay_mesh.vertices, mesh.vertices),
            (replay_mesh.triangles, mesh.triangles),
            (replay_anchor, anchor),
            (replay_rows, directions),
            (replay_bounds, bounds),
            (replay_primary, primary),
        ):
            np.testing.assert_array_equal(actual, expected)
        replays.append(
            {
                "seed": source_key,
                "tiles": len(minimal),
                "same_version_geometry_and_solver_inputs": True,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "scope": __doc__,
        "owner": domain.owner,
        "version": domain.version,
        "native_source_indices": native_indices,
        "anchor_lon_lat": anchor.tolist(),
        "target_geodetic_model_height_m": target,
        "original_source_membership": csr.work,
        "topology": topology,
        "physical_fit": dict(solved.report),
        "fit_cost": cost,
        "independent_audit": audit_report,
        "regional_boundary": error,
        "replays": replays,
        "raw_source_closure": False,
        "open_river_continuation_contacts": False,
    }
    (output / "fit.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        output / "fit.npz",
        vertices=solved.mesh.vertices,
        triangles=solved.mesh.triangles,
        anchor=anchor,
        source_offsets=csr.offsets,
        source_members=csr.members,
        original_native_surface_indices=native_indices,
        cut_vertices=cut.vertices,
        cut_seam=cut.seam,
        left_faces=cut.faces[0],
        right_faces=cut.faces[1],
        left_parents=cut.parents[0],
        right_parents=cut.parents[1],
        cut_vertices_ecef=to_world(anchor, cut.vertices),
    )
    plot(solved.mesh, polygons, cut, output / "fit.png")
    print(json.dumps(report, indent=2))


def plot(mesh, polygons, cut, path):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(11, 5))
    for polygon in polygons:
        for ring in (polygon.exterior, *polygon.interiors):
            axes[0].plot(*np.asarray(ring.coords).T, linewidth=1)
    axes[0].triplot(*mesh.vertices[:, :2].T, mesh.triangles, color="0.5", linewidth=0.5)
    axes[0].set_title("Original source rings\nCanonical triangulation")
    colors = axes[1].tripcolor(
        *mesh.vertices[:, :2].T, mesh.triangles, mesh.vertices[:, 2], shading="gouraud"
    )
    for edge in cut.seam:
        axes[1].plot(*cut.vertices[edge, :2].T, color="red", linewidth=2)
    axes[1].set_title("Physical lake fit\nShared regional cut (red)")
    figure.colorbar(colors, ax=axes[1], label="Canonical ENU Up (m)", format="%.3f")
    for axis in axes:
        axis.set_aspect("equal")
        axis.set_xlabel("Canonical east (m)")
        axis.set_ylabel("North (m)")
    figure.suptitle(
        "Conditional admitted-fixture lake; raw source completeness unproven"
    )
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    run(arguments.fixture, arguments.inventory, arguments.output)
