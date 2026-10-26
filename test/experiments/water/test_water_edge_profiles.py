"""Analytic whole-edge restrictions and complete shared-water interval controls."""

import dataclasses
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from compare_water_edge_profiles import consume_saved_profile
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from test_owned_water_cells import cell
from water_cell_edges import align_edges, edge_knots
from water_cell_handoff import route_cells
from water_edge_profile_audit import audit_profile
from water_edge_profiles import prepare_profile_owner, profile_border, restrict_profile
from water_shared_borders import shared_borders

A, B = (4, 5, 5), (4, 6, 5)


def borders(polygons):
    snapshots = {A: cell(A, polygons[0]), B: cell(B, polygons[1])}
    products, _ = route_cells(
        snapshots, source_tiles=(A, B), recipients=(A, B), scope="edge-profile-test-v1"
    )
    knots = edge_knots(products)
    products, _ = align_edges(products, knots)
    records = [
        (t, p, c.parents[t + (p.feature, p.part)], c.snapshot.source)
        for t, c in products.items()
        for p in c.snapshot.pieces
    ]
    return records, knots


def owner():
    return Mesh(
        [[0, 0, 500], [1000, 0, 500], [1000, 1000, 505], [0, 1000, 500]],
        [[0, 1, 2], [0, 2, 3]],
    )


class WaterEdgeProfiles(unittest.TestCase):
    def test_saved_neighbors_require_the_same_owner_and_edge(self):
        mesh = owner()
        prepared = prepare_profile_owner(
            mesh,
            anchor=[9.6, 47.2],
            mask=shapely.box(0, 0, 1000, 1000),
            owner_version="import-r1",
        )
        profile = restrict_profile(prepared, [np.array([[250, 0], [250, 1000]])])
        self.assertFalse(prepared.anchor.flags.writeable)
        with tempfile.TemporaryDirectory(
            prefix="outshine-water-edge-consumers-"
        ) as tmp:
            path = Path(tmp) / "profile.npz"
            edge = (4, 0, 6, 5)
            np.savez_compressed(
                path,
                points=profile.points,
                segments=profile.segments,
                context=np.asarray(profile.context),
                edge=np.asarray(edge),
            )
            consume_saved_profile(path, prepared, edge)
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                consume_saved_profile(path, prepared, (4, 1, 6, 5))
            with self.assertRaisesRegex(ValueError, "context mismatch"):
                consume_saved_profile(
                    path, dataclasses.replace(prepared, context="other-root"), edge
                )

    def test_endpoint_agreement_misses_an_interior_root_crease(self):
        mesh = owner()
        line = np.array([[250, 0], [250, 1000]])
        profile = profile_border(
            mesh,
            [line],
            anchor=[9.6, 47.2],
            mask=shapely.box(0, 0, 1000, 1000),
            owner_version="root-r1",
        )
        self.assertEqual(len(profile.segments), 2)
        np.testing.assert_array_equal(
            profile.points, [[250, 0, 500], [250, 250, 501.25], [250, 1000, 501.25]]
        )
        # Endpoints alone interpolate500.3125 at y250, missing the real501.25.
        self.assertEqual(profile.points[1, 2] - (500 + 0.25 * 1.25), 0.9375)
        fractions = np.linspace(0, 1, 17)
        for edge in profile.segments:
            p = profile.points[edge]
            interpolated = p[0] + fractions[:, None] * (p[1] - p[0])
            truth = 500 + 0.005 * np.minimum(interpolated[:, 0], interpolated[:, 1])
            np.testing.assert_allclose(interpolated[:, 2], truth, atol=1e-12, rtol=0)
        self.assertFalse(profile.points.flags.writeable)
        self.assertEqual(profile.report["maximum_parent_plane_up_error_m"], 0)
        self.assertEqual(
            audit_profile(mesh, [line], profile)[
                "maximum_complete_edge_up_difference_m"
            ],
            0,
        )
        endpoint_only = dataclasses.replace(
            profile,
            points=profile.points[[0, -1]],
            segments=np.array([[0, 1]], np.uint32),
            parent_faces=np.array([0]),
            report=dict(profile.report, source_segment_addresses=[(0, 0)]),
        )
        with self.assertRaisesRegex(ValueError, "piecewise-affine"):
            audit_profile(mesh, [line], endpoint_only)
        missing = dataclasses.replace(
            profile,
            segments=profile.segments[1:],
            parent_faces=profile.parent_faces[1:],
            report=dict(profile.report, source_segment_addresses=[(0, 0)]),
        )
        with self.assertRaisesRegex(ValueError, "endpoints"):
            audit_profile(mesh, [line], missing)

    def test_root_edge_coincidence_does_not_duplicate_the_profile(self):
        mesh = owner()
        p = profile_border(
            mesh,
            [np.array([[0, 0], [1000, 1000]])],
            anchor=[0, 0],
            mask=shapely.box(0, 0, 1000, 1000),
            owner_version="diagonal",
        )
        self.assertEqual(len(p.segments), 1)
        np.testing.assert_array_equal(p.points[:, 2], [500, 505])

    def test_disjoint_water_intervals_never_fill_an_island_gap(self):
        left = shapely.Polygon(
            [
                (5.2, 5.2),
                (6, 5.2),
                (6, 5.4),
                (5.7, 5.4),
                (5.7, 5.6),
                (6, 5.6),
                (6, 5.8),
                (5.2, 5.8),
            ]
        )
        right = shapely.Polygon(
            [
                (6, 5.2),
                (6.8, 5.2),
                (6.8, 5.8),
                (6, 5.8),
                (6, 5.6),
                (6.3, 5.6),
                (6.3, 5.4),
                (6, 5.4),
            ]
        )
        records, knots = borders([left, right])
        products, report = shared_borders(records, knots, known_cells=(A, B))
        self.assertEqual(report["shared_edges"], 1)
        self.assertEqual(len(products[0]["polylines"]), 2)
        intervals = [
            (float(p[:, 1].min()), float(p[:, 1].max()))
            for p in products[0]["polylines"]
        ]
        self.assertEqual(intervals, [(5.2, 5.4), (5.6, 5.8)])
        self.assertEqual(len(products[0]["source_keys"]), 2)

    def test_point_only_contacts_and_unknown_neighbors_are_not_complete_lines(self):
        records, knots = borders(
            [shapely.box(5.2, 5.2, 6, 5.4), shapely.box(6, 5.4, 6.8, 5.8)]
        )
        products, _ = shared_borders(records, knots, known_cells=(A, B))
        self.assertFalse(products)
        selected = [r for r in records if r[0] == A]
        products, report = shared_borders(selected, knots, known_cells=(A,))
        self.assertFalse(products)
        self.assertEqual(report["open_edges"][0]["unknown_cells"], (B,))

    def test_coastal_three_cell_corner_keeps_unknown_fourth_cell_open(self):
        c = (4, 5, 6)
        raw = shapely.box(5.8, 5.8, 6.2, 6.2)
        snapshots = {t: cell(t, raw) for t in (A, B, c)}
        snapshots = {
            t: dataclasses.replace(
                s,
                pieces=tuple(
                    dataclasses.replace(p, info=dict(p.info, kind="ocean"))
                    for p in s.pieces
                ),
            )
            for t, s in snapshots.items()
        }
        products, _ = route_cells(
            snapshots,
            source_tiles=snapshots,
            recipients=snapshots,
            scope="coastal-corner-v1",
        )
        knots = edge_knots(products)
        products, _ = align_edges(products, knots)
        records = [
            (t, p, owner.parents[t + (p.feature, p.part)], owner.snapshot.source)
            for t, owner in products.items()
            for p in owner.snapshot.pieces
        ]
        curves, status = shared_borders(records, knots, known_cells=snapshots)
        self.assertEqual({r["edge"] for r in curves}, {(4, 0, 6, 5), (4, 1, 5, 6)})
        self.assertEqual(len(status["open_edges"]), 2)
        self.assertTrue(
            all(r["unknown_cells"] == ((4, 6, 6),) for r in status["open_edges"])
        )
        for curve in curves:
            self.assertTrue(
                any(
                    np.array_equal(point, [6, 6])
                    for line in curve["polylines"]
                    for point in line
                )
            )

    def test_gap_in_owner_or_mask_refuses_without_snapping(self):
        mesh = owner()
        line = np.array([[250, 0], [250, 1000]])
        for mask, field in (
            (shapely.box(0, 0, 100, 100), mesh),
            (shapely.box(0, 0, 1000, 1000), Mesh(mesh.vertices, mesh.triangles[:1])),
        ):
            with self.assertRaises(ValueError):
                profile_border(
                    field, [line], anchor=[0, 0], mask=mask, owner_version="incomplete"
                )
        with self.assertRaisesRegex(ValueError, "completely covered"):
            profile_border(
                Mesh(mesh.vertices, mesh.triangles[:1]),
                [np.array([[10, 900], [20, 950]])],
                anchor=[0, 0],
                mask=shapely.box(0, 0, 1000, 1000),
                owner_version="empty-bbox-candidate",
            )

    def test_order_frame_version_and_work_caps_are_explicit(self):
        mesh = owner()
        line = np.array([[250, 0], [250, 1000]])
        kwargs = {
            "anchor": [9.6, 47.2],
            "mask": shapely.box(0, 0, 1000, 1000),
            "owner_version": "r1",
        }
        first = profile_border(mesh, [line], **kwargs)
        other = profile_border(mesh, [line], **dict(kwargs, owner_version="r2"))
        self.assertNotEqual(first.context, other.context)
        moved = profile_border(mesh, [line], **dict(kwargs, anchor=[9.7, 47.2]))
        self.assertNotEqual(first.context, moved.context)
        for caps in ({"segment_limit": 1}, {"pair_limit": 1}):
            with self.assertRaisesRegex(ValueError, "budget"):
                profile_border(mesh, [line], **kwargs, **caps)
        reverse = profile_border(mesh, [line[::-1]], **kwargs)
        sampler = SurfaceSampler(mesh, xy_tolerance_m=0)
        sampled = sampler.sample(reverse.points[:, :2])
        np.testing.assert_array_equal(reverse.points, sampled.points_enu_m)

    def test_geometric_detour_is_not_a_complete_source_edge(self):
        mesh = owner()
        flat = Mesh(np.c_[mesh.vertices[:, :2], np.full(4, 500)], mesh.triangles)
        line = np.array([[250, 0], [250, 1000]])
        profile = profile_border(
            flat,
            [line],
            anchor=[0, 0],
            mask=shapely.box(0, 0, 1000, 1000),
            owner_version="flat",
        )
        moved = profile.points.copy()
        moved[1, 0] += 50
        with self.assertRaisesRegex(ValueError, "moved away"):
            audit_profile(flat, [line], dataclasses.replace(profile, points=moved))


if __name__ == "__main__":
    unittest.main()
