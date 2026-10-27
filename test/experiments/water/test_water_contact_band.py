"""Shared full-facet contacts, constant-mode ownership and source protection."""

import dataclasses
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.water.frame import model_height_targets
from water_contact_band import (
    LevelMoment,
    common_level_shift,
    compose,
    crossing_faces,
    level_moment,
    physical_audit,
    solve_band,
    solve_region,
)
from water_contact_inputs import roundoff


def grid(anchor=(9.6, 47.23), height=500):
    xy = np.array(
        [(x, y) for y in (-30.0, -10.0, 10.0, 30.0) for x in np.linspace(-80, 80, 17)]
    )
    triangles = []
    for j in range(3):
        for i in range(16):
            a = j * 17 + i
            triangles.extend(((a, a + 1, a + 18), (a, a + 18, a + 17)))
    target = np.full(len(xy), height) - 0.002 * xy[:, 0] + 0.001 * xy[:, 1]
    mesh = Mesh(
        np.c_[xy, model_height_targets(anchor, xy, target)],
        np.asarray(triangles, dtype=np.uint32),
    )
    count = len(triangles)
    rows = np.r_[
        np.c_[np.arange(count), np.tile([1, 0, 0, -0.01, 0.01], (count, 1))],
        np.c_[np.arange(count), np.tile([0, 1, 0, -0.01, 0.01], (count, 1))],
    ]
    return (
        mesh,
        np.array(anchor),
        rows,
        [float(mesh.vertices[:, 2].min() - 20), float(mesh.vertices[:, 2].max() + 20)],
    )


class ContactBandTests(unittest.TestCase):
    def test_full_crossing_facets_are_owned_and_capacity_rejects(self):
        mesh, *_ = grid()
        faces = crossing_faces(mesh, 0, 10)
        crossing = np.flatnonzero(
            (mesh.vertices[mesh.triangles, 0].min(1) <= 0)
            & (mesh.vertices[mesh.triangles, 0].max(1) >= 0)
        )
        self.assertTrue(np.isin(crossing, faces).all())
        with self.assertRaises(ValueError):
            crossing_faces(mesh, 0, 10, face_limit=1)

    def test_changed_priors_keep_owner_bits_and_composed_field_is_admitted(self):
        mesh, anchor, rows, bounds = grid()
        band, _ = solve_band(
            mesh,
            anchor,
            rows,
            bounds,
            plane_x=0,
            half_width=10,
            recipe_revision="synthetic-v1",
        )
        self.assertIsNotNone(band)
        regions = {}
        for side in (1, 0):
            result, domain, report = solve_region(
                mesh,
                anchor,
                rows,
                bounds,
                band,
                plane_x=0,
                side=side,
                prior_offset_m=0.25 if side else -0.25,
            )
            self.assertIsNotNone(result, report)
            indices = np.searchsorted(domain.vertex_ids, band.domain.vertex_ids)
            np.testing.assert_array_equal(result.vertices[indices], band.mesh.vertices)
            regions[side] = (result, domain)
        joined, report = compose(
            mesh, anchor, rows, bounds, band, [regions[0], regions[1]], plane_x=0
        )
        self.assertTrue(report["admitted"])
        np.testing.assert_array_equal(
            joined.vertices[band.domain.vertex_ids], band.mesh.vertices
        )
        self.assertFalse(band.domain.vertex_ids.flags.writeable)

    def test_stale_topology_and_incompatible_contacts_are_explicit(self):
        mesh, anchor, rows, bounds = grid()
        band, _ = solve_band(
            mesh,
            anchor,
            rows,
            bounds,
            plane_x=0,
            half_width=10,
            recipe_revision="synthetic-v1",
        )
        vertices = mesh.vertices.copy()
        vertices[0, 0] -= 1
        with self.assertRaises(ValueError):
            solve_region(
                Mesh(vertices, mesh.triangles),
                anchor,
                rows,
                bounds,
                band,
                plane_x=0,
                side=0,
            )
        with self.assertRaises(ValueError):
            solve_region(mesh, anchor + [1, 0], rows, bounds, band, plane_x=0, side=0)
        broken = band.mesh.vertices.copy()
        broken[:, 2] += 100
        broken_band = dataclasses.replace(band, mesh=Mesh(broken, band.mesh.triangles))
        result, _domain, _report = solve_region(
            mesh, anchor, rows, bounds, broken_band, plane_x=0, side=0
        )
        self.assertIsNone(result)

    def test_exact_constant_mode_matches_dense_triangle_integral(self):
        mesh, _anchor, _rows, bounds = grid()
        field = Mesh(mesh.vertices + [0, 0, -1.25], mesh.triangles)
        ids = np.arange(len(mesh.triangles))
        moments = {}
        for k, faces in enumerate(np.array_split(ids, 3)):
            moments[k] = level_moment(mesh, field, faces, bounds)
        shift, report = common_level_shift(moments, free_level=True)
        self.assertAlmostEqual(shift, 1.25, places=12)
        self.assertEqual(report["source_faces_counted_once"], len(ids))
        reversed_shift, _ = common_level_shift(
            dict(reversed(list(moments.items()))), free_level=True
        )
        self.assertEqual(shift, reversed_shift)
        points = mesh.vertices[mesh.triangles, :2]
        a = points[:, 1] - points[:, 0]
        b = points[:, 2] - points[:, 0]
        area = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
        residual = mesh.vertices[:, 2] - field.vertices[:, 2]

        def integral(delta):
            e = residual[mesh.triangles] - delta
            return np.sum(
                area
                / 6
                * (
                    (e * e).sum(1)
                    + e[:, 0] * e[:, 1]
                    + e[:, 0] * e[:, 2]
                    + e[:, 1] * e[:, 2]
                )
            )

        self.assertLessEqual(integral(shift), integral(shift - 0.01))
        self.assertLessEqual(integral(shift), integral(shift + 0.01))

    def test_hard_datum_prohibits_shift_and_height_interval_clamps(self):
        m = {0: LevelMoment(2, 4, -0.5, 0.5, 1)}
        self.assertEqual(common_level_shift(m, free_level=False)[0], 0)
        self.assertEqual(common_level_shift(m, free_level=True)[0], 0.5)
        with self.assertRaises(ValueError):
            common_level_shift({}, free_level=True)

    def test_negative_height_and_polar_frame_stay_physically_admitted(self):
        for anchor, height in (((0, 82), 500), ((120, 0), -300)):
            mesh, a, rows, bounds = grid(anchor, height)
            band, report = solve_band(
                mesh,
                a,
                rows,
                bounds,
                plane_x=0,
                half_width=10,
                recipe_revision="adverse-v1",
            )
            self.assertIsNotNone(band, report)
            self.assertTrue(
                physical_audit(band.mesh, a, band.domain.rows, bounds)["admitted"]
            )

    def test_original_hole_contacts_prohibit_ulp_contraction(self):
        x = -100.0
        near = np.nextafter(x, 0)
        vertices = np.array(
            [
                [x, 0, 500],
                [near, 0, 500],
                [x, -10, 500],
                [-5, -10, 500],
                [5, -10, 500],
                [5, -5, 500],
            ]
        )
        mesh = Mesh(vertices, np.array([[0, 2, 1], [3, 4, 5]], dtype=np.uint32))
        rows = np.r_[
            np.c_[np.arange(2), np.tile([1, 0, 0, -0.01, 0.01], (2, 1))],
            np.c_[np.arange(2), np.tile([0, 1, 0, -0.01, 0.01], (2, 1))],
        ]
        source = shapely.Polygon(
            [(-120, -20), (20, -20), (20, 20), (-120, 20)],
            holes=[[(-100, 0), (-90, 0), (-90, 10), (-100, 10)]],
        )
        with self.assertRaisesRegex(ValueError, "protected"):
            roundoff(mesh, rows, [source])


if __name__ == "__main__":
    unittest.main()
