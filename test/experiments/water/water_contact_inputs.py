"""Original source provenance -> fixed bounded Ill pair, before any height fit."""

import numpy as np
import shapely
from compare_native_water_frame import course_controls, original_case
from native_river_inputs import surface_rings
from outshine_ref.contracts import Mesh
from outshine_ref.water.frame import model_height_targets
from outshine_ref.water.geodetic import local_points
from river_membership import face_membership
from river_profiles import RiverProfile
from river_surface import surface_mesh
from water_contact_band import crossing_faces
from water_sliver_edge import contract_edge


def roundoff(mesh, rows, polygons):
    """Existing #935 single-edge experiment, native one-ULP policy, explicit guards."""
    reports = []
    old = face_membership(mesh.vertices, mesh.triangles, polygons)
    source_ids = np.unique(old.members)
    for step in range(5):
        edges = np.unique(
            np.sort(
                np.concatenate(
                    [
                        mesh.triangles[:, [0, 1]],
                        mesh.triangles[:, [1, 2]],
                        mesh.triangles[:, [2, 0]],
                    ]
                ),
                axis=1,
            ),
            axis=0,
        )
        points = mesh.vertices[edges, :2]
        adjacent = np.all(
            (points[:, 0] == points[:, 1])
            | (np.nextafter(points[:, 0], points[:, 1]) == points[:, 1]),
            axis=1,
        )
        candidates = edges[adjacent]
        if not len(candidates):
            return mesh, rows, old, reports
        if step == 4:
            raise ValueError("bounded one-ULP intervention budget exhausted")
        protected = set(np.unique(mesh.triangles[crossing_faces(mesh, 0, 20)]).tolist())
        # Protect original hole contacts, including tiny ones; union artifacts are not source rings.
        for part in polygons:
            for hole in part.interiors:
                a, b, c, d = hole.bounds
                xy = mesh.vertices[:, :2]
                protected.update(
                    np.flatnonzero(
                        (xy[:, 0] >= a)
                        & (xy[:, 0] <= c)
                        & (xy[:, 1] >= b)
                        & (xy[:, 1] <= d)
                    ).tolist()
                )
        pair = candidates[0]
        n = len(mesh.triangles)
        contracted = contract_edge(
            mesh,
            rows,
            np.c_[np.arange(n), n + np.arange(n)],
            pair,
            maximum_shift_m=float(
                np.linalg.norm(mesh.vertices[pair[0], :2] - mesh.vertices[pair[1], :2])
            ),
            protected=protected,
        )
        new = face_membership(
            contracted.mesh.vertices, contracted.mesh.triangles, polygons
        )
        if np.any(new.status) or not np.array_equal(source_ids, np.unique(new.members)):
            raise ValueError(
                "one-ULP contraction loses positive original source coverage"
            )
        reports.append(
            dict(
                contracted.report,
                protected_vertices=len(protected),
                all_original_source_ids_retained=True,
            )
        )
        mesh, rows, old = contracted.mesh, contracted.rows, new
    raise AssertionError("unreachable bounded intervention loop")


def original_pair(path):
    data, manifest, legacy = original_case(path)
    anchor = np.asarray(legacy["frame"]["origin_lon_lat"])
    selection = manifest["original_selection"]
    window = shapely.box(-150, -300, 150, 300)
    originals, clipped, ledger = [], [], []
    for index in selection["selected_surface_indices"]:
        rings = [
            local_points(anchor, r[:, ::-1], 0)[:, :2]
            for r in surface_rings(data, index)
        ]
        polygon = shapely.Polygon(rings[0], rings[1:])
        originals.append(polygon)
        piece = polygon.intersection(window)
        used = not piece.is_empty and piece.area > 0
        ledger.append(
            {
                "original_surface": index,
                "reason": "in_canonical_pair" if used else "outside_canonical_pair",
            }
        )
        if used:
            clipped.append(piece)
    body = shapely.union_all(clipped)
    flat, topology = surface_mesh(body)
    courses, course_report = course_controls(
        data, selection["selected_course_indices"], anchor, np.ones(2)
    )
    geographic = courses["course_original_lon_lat"]
    segments = local_points(anchor, geographic.reshape(-1, 2), 0).reshape(-1, 2, 3)
    target, references = RiverProfile(
        segments[:, :, :2], courses["course_original_levels_m"]
    ).sample(flat.vertices[:, :2], "blend-affine")
    up = model_height_targets(anchor, flat.vertices[:, :2], target)
    mesh = Mesh(np.c_[flat.vertices[:, :2], up], flat.triangles)
    count = len(mesh.triangles)
    # Symmetric laboratory caps, no DEM-based hard flow-direction assumption.
    rows = np.r_[
        np.c_[np.arange(count), np.tile([1, 0, 0, -0.01, 0.01], (count, 1))],
        np.c_[np.arange(count), np.tile([0, 1, 0, -0.01, 0.01], (count, 1))],
    ]
    bounds = [float(up.min() - 20), float(up.max() + 20)]
    mesh, rows, csr, interventions = roundoff(mesh, rows, originals)
    return (
        mesh,
        anchor,
        rows,
        bounds,
        csr,
        {
            "scope": "bounded fixed pair in existing original Ill fixture; conditional admitted-source window, no world closure or source-completeness proof",
            "canonical_pair_bounds_enu_m": [-150, -300, 150, 300],
            "historical_native_sha256": manifest["historical_v2"]["sha256"],
            "original_source_ledger": ledger,
            "projected_source_surfaces": len(originals),
            "native_surfaces_decoded": len(data["surfaces"]),
            "native_courses_decoded": len(data["courses"]),
            "native_points_decoded": len(data["points_lat_lon"]),
            "native_input_bytes": data["source_bytes"],
            "original_source_coordinates": int(
                shapely.get_num_coordinates(originals).sum()
            ),
            "positive_clipped_sources": len(clipped),
            "original_source_holes": sum(len(p.interiors) for p in originals),
            "derived_pair_holes_before_roundoff": sum(
                len(p.interiors) for p in shapely.get_parts(body)
            ),
            "topology_before_roundoff": topology,
            "roundoff": interventions,
            "membership": csr.work,
            "course_sampling": course_report,
            "course_reference_visits": int(references.sum()),
            "original_native_surface_indices": selection["selected_surface_indices"],
            "model": "existing blend-affine courses as soft numerical model heights; no observed ASL or flow direction",
            "lab_symmetric_physical_grade_cap": 0.01,
            "contact_plane_x_m": 0,
            "band_half_width_m": 20,
        },
    )
