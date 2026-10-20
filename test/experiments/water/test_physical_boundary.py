"""Geometric controls for the cross-frame water-edge decision (#941)."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from compare_physical_boundary import synthetic
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import ecef
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import normal_caps, tangent_directions
from physical_boundary import (
    boundary_error,
    canonical_cut,
    child_rows,
    cut_values,
    local_piece,
    signed_twice_area,
    to_world,
)
from physical_boundary_audit import inherited_envelope, read_piece, store_piece


class PhysicalBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.mesh, self.anchor, self.rows, self.box = synthetic([9.6, 47.23], 500)

    def test_affine_cut_preserves_areas_and_plane_per_parent(self):
        cut = canonical_cut(self.mesh, 10)
        areas = []
        for faces, parents in zip(cut.faces, cut.parents, strict=True):
            area = signed_twice_area(cut.vertices, faces) / 2
            areas.append(area.sum())
            points = cut.vertices[faces]
            original = self.mesh.vertices[self.mesh.triangles[parents]]
            normals = np.cross(
                original[:, 1] - original[:, 0], original[:, 2] - original[:, 0]
            )
            distances = np.einsum("fki,fi->fk", points - original[:, :1], normals)
            np.testing.assert_allclose(distances, 0, rtol=0, atol=2e-9)
        np.testing.assert_array_equal(areas, [5400, 4200])

    def test_cut_at_existing_vertices_and_edges_has_no_t_junction(self):
        cut = canonical_cut(self.mesh, 0)
        self.assertEqual(len(cut.crossing_edges), 0)
        self.assertEqual(len(cut.seam), 2)
        for side in range(2):
            self.assertTrue(np.isin(cut.seam, cut.faces[side]).all())

    def test_interpolation_keeps_same_cut_on_a_second_affine_field(self):
        cut = canonical_cut(self.mesh, 10)
        field = self.mesh.vertices.copy()
        field[:, 2] = 7 + 0.01 * field[:, 0] - 0.02 * field[:, 1]
        result = cut_values(cut, field)
        np.testing.assert_allclose(
            result[:, 2],
            7 + 0.01 * result[:, 0] - 0.02 * result[:, 1],
            rtol=0,
            atol=2e-15,
        )

    def test_separate_frames_share_every_straight_subedge(self):
        cut = canonical_cut(self.mesh, 10)
        anchors = ([9.59, 47.23], [9.61, 47.23])
        pieces = [
            local_piece(cut, side, self.anchor, a) for side, a in enumerate(anchors)
        ]
        result = boundary_error(
            pieces[0][0],
            anchors[0],
            pieces[0][2],
            pieces[1][0],
            anchors[1],
            pieces[1][2],
        )
        self.assertLess(result["whole_segment_sample_max_m"], 2e-9)

    def test_rows_preserve_all_parent_constraints_and_world_directions(self):
        cut = canonical_cut(self.mesh, 10)
        rows, primary, gathered = child_rows(
            self.rows, cut.parents[0], self.anchor, [10, 48], 8
        )
        self.assertEqual(len(rows), 2 * len(cut.faces[0]))
        np.testing.assert_array_equal(
            rows[primary[:, 0], 0], np.arange(len(cut.faces[0]))
        )
        np.testing.assert_array_equal(
            rows[primary[:, 1], 4:6], self.rows[8:, 4:6][cut.parents[0]]
        )
        # Endpoints minus origins independently check the vector rotation.
        before = to_world(self.anchor, self.rows[gathered, 1:4]) - to_world(
            self.anchor, np.zeros((len(rows), 3))
        )
        after = to_world([10, 48], rows[:, 1:4]) - to_world(
            [10, 48], np.zeros((len(rows), 3))
        )
        np.testing.assert_allclose(before, after, rtol=0, atol=2e-9)

    def test_endpoint_equality_does_not_prove_a_different_tessellation(self):
        geo = np.array([[9.6, 47.23], [9.605, 47.23], [9.61, 47.23]])
        curve = ecef(geo[:, 1], geo[:, 0], 500)
        chord = (curve[0] + curve[2]) / 2
        self.assertGreater(np.linalg.norm(curve[1] - chord), 0.016)

    def test_inherited_bound_requires_whole_child_containment(self):
        cut = canonical_cut(self.mesh, 10)
        caps = normal_caps(
            self.anchor, self.mesh.vertices[:, :2], self.mesh.triangles, self.box
        )
        rows, _primary, gathered = child_rows(
            self.rows, cut.parents[0], self.anchor, self.anchor, 8
        )
        tangent = tangent_directions(
            caps["up"][self.rows[:, 0].astype(int)], self.rows[:, 1:4]
        )[gathered]
        good = inherited_envelope(
            cut.vertices, cut.faces[0], cut.parents[0], caps, rows, tangent
        )
        self.assertTrue(good["certified"])
        changed = cut.vertices.copy()
        changed[:, 2] += 100
        bad = inherited_envelope(
            changed, cut.faces[0], cut.parents[0], caps, rows, tangent
        )
        self.assertFalse(bad["certified"])
        self.assertGreater(bad["parent_up_box_violation_m"], 0)

    def test_boundary_budget_refuses_without_modifying_input(self):
        before = self.mesh.vertices.copy()
        with self.assertRaisesRegex(ValueError, "cut_boundary_budget"):
            canonical_cut(self.mesh, 10, boundary_limit=2)
        np.testing.assert_array_equal(self.mesh.vertices, before)

    def test_unsupported_projection_refuses_only_that_piece(self):
        cut = canonical_cut(self.mesh, 10)
        with self.assertRaisesRegex(ValueError, "local_projection_fold_or_roundoff"):
            local_piece(cut, 0, self.anchor, [-170.4, -47.23])
        healthy, _ids, _seam = local_piece(cut, 1, self.anchor, self.anchor)
        self.assertGreater(len(healthy.triangles), 0)

    def test_invalid_and_empty_local_cuts_are_explicit(self):
        for plane, reason in ((np.nan, "finite plane"), (1000, "empty_cut_half")):
            with self.assertRaisesRegex(ValueError, reason):
                canonical_cut(self.mesh, plane)
        flat = Mesh(
            np.array([[0.0, 0, 0], [1, 0, 0], [2, 0, 0]]), np.array([[0, 1, 2]])
        )
        with self.assertRaisesRegex(ValueError, "degenerate_input"):
            canonical_cut(flat, 0.5)

    def test_incompatible_neighbor_is_local_and_does_not_change_good_fit(self):
        primary = np.c_[np.arange(8), 8 + np.arange(8)]
        bad = fit_physical_surface(
            self.mesh,
            self.anchor,
            self.rows,
            up_bounds_m=self.box,
            fixed={0: float(self.box[1] + 100)},
            primary_rows=primary,
        )
        self.assertIsNone(bad.mesh)
        self.assertIn(
            bad.report["solver_status"], ("PrimalInfeasible", "AlmostPrimalInfeasible")
        )
        good = fit_physical_surface(
            self.mesh,
            self.anchor,
            self.rows,
            up_bounds_m=self.box,
            primary_rows=primary,
        )
        self.assertIsNotNone(good.mesh)
        self.assertTrue(good.report["finite_qp_optimal"])

    def test_one_stored_product_needs_no_body_and_detects_stale_boundary(self):
        cut = canonical_cut(self.mesh, 10)
        local, ids, seam = local_piece(cut, 0, self.anchor, [9.59, 47.23])
        boundary = np.unique(cut.seam)
        with tempfile.TemporaryDirectory(prefix="outshine-tests.boundary.") as tmp:
            path = Path(tmp) / "piece.npz"
            store_piece(
                path,
                local,
                [9.59, 47.23],
                ids,
                seam,
                cut.parents[0],
                to_world(self.anchor, cut.vertices[boundary]),
                boundary,
                np.searchsorted(boundary, cut.seam).astype(np.uint32),
            )
            self.assertEqual(read_piece(path)["boundary_segments"], len(seam))
            with self.assertRaisesRegex(ValueError, "boundary_version_mismatch"):
                read_piece(path, expected_version="obsolete-neighbor")
            with np.load(path) as data:
                broken = dict(data)
            broken["boundary_ecef"][0, 2] += 1
            np.savez_compressed(path, **broken)
            with self.assertRaisesRegex(ValueError, "boundary_version_mismatch"):
                read_piece(path)


if __name__ == "__main__":
    unittest.main()
