"""Independent solids, whole/partial watertightness and pre-expansion limits."""

import unittest
from itertools import combinations, pairwise
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.buildings.profiled_bands import (
    BandPlan,
    band_surface,
    edge_footprints,
)
from shapely.affinity import rotate, translate

PROFILE = (
    (0.0, 0.0),
    (0.08, 0.0),
    (0.12, 0.08),
    (0.2, 0.08),
    (0.2, 0.2),
    (0.1, 0.2),
    (0.1, 0.3),
    (0.0, 0.3),
)


def solid(test, mesh):
    points = mesh["vertices"][mesh["faces"]]
    n = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    test.assertTrue(np.all(np.linalg.norm(n, axis=1) > 1e-12))
    np.testing.assert_allclose(
        n / np.linalg.norm(n, axis=1, keepdims=True),
        mesh["normals"][mesh["faces"]][:, 0],
        atol=1e-12,
    )
    directed = {}
    for triangle in np.round(points, 9):
        for a, b in zip(triangle, np.roll(triangle, -1, axis=0), strict=True):
            a, b = tuple(a), tuple(b)
            directed[(a, b)] = directed.get((a, b), 0) + 1
    for (a, b), count in directed.items():
        test.assertEqual(count, 1)
        test.assertEqual(directed.get((b, a)), 1)
    uv = mesh["uv"][mesh["faces"]]
    side3 = points[:, 1:] - points[:, :1]
    side2 = uv[:, 1:] - uv[:, :1]
    np.testing.assert_allclose(
        np.linalg.norm(side3, axis=2), np.linalg.norm(side2, axis=2), atol=1e-10
    )
    centre = mesh["vertices"].mean(axis=0)
    points = points - centre
    return float(
        np.einsum("ij,ij->i", points[:, 0], np.cross(points[:, 1], points[:, 2])).sum()
        / 6
    )


def section_volume(polygon):
    value = 0.0
    for (r0, z0), (r1, z1) in pairwise(PROFILE):
        areas = [
            polygon.buffer(r, join_style=2).area - polygon.area
            for r in (r0, (r0 + r1) / 2, r1)
        ]
        value += (z1 - z0) * (areas[0] + 4 * areas[1] + areas[2]) / 6
    return value


def horizontal_slice(mesh, z):
    lines = []
    for triangle in mesh["vertices"][mesh["faces"]]:
        intersections = []
        for a, b in zip(triangle, np.roll(triangle, -1, axis=0), strict=True):
            if (a[2] < z < b[2]) or (b[2] < z < a[2]):
                intersections.append(
                    a[:2] + (b[:2] - a[:2]) * (z - a[2]) / (b[2] - a[2])
                )
        if intersections:
            if len(intersections) != 2:
                raise AssertionError("Non-vertex plane should cut two triangle edges")
            lines.append(shapely.LineString(np.round(intersections, 8)))
    cells = list(
        shapely.get_parts(
            shapely.polygonize(list(shapely.get_parts(shapely.union_all(lines))))
        )
    )
    loops = {}
    for cell in cells:
        for ring in (cell.exterior, *cell.interiors):
            p = shapely.Polygon(ring)
            loops[shapely.normalize(p).wkb] = p
    kept = [
        cell
        for cell in cells
        if sum(p.contains(cell.representative_point()) for p in loops.values()) % 2
    ]
    return shapely.union_all(kept)


class ProfileBands(unittest.TestCase):
    def test_rectangle_concave_and_courtyard_volumes(self):
        polygons = (
            shapely.box(0, 0, 12, 18),
            shapely.Polygon([(0, 0), (20, 0), (20, 8), (8, 8), (8, 18), (0, 18)]),
            shapely.Polygon(
                [(0, 0), (40, 0), (40, 30), (0, 30)],
                [[(12, 8), (28, 8), (28, 22), (12, 22)]],
            ),
        )
        for index, polygon in enumerate(polygons):
            polygon = translate(rotate(polygon, 27), -130, 210)
            plan = BandPlan(polygon, PROFILE, base_m=19)
            mesh = band_surface(plan)
            self.assertAlmostEqual(solid(self, mesh), section_volume(polygon), places=8)
            if index == 0:
                analytic = 0.0
                for (r0, z0), (r1, z1) in pairwise(PROFILE):
                    mean_radius = (r0 + r1) / 2
                    mean_square = (r0 * r0 + r0 * r1 + r1 * r1) / 3
                    analytic += (z1 - z0) * (60 * mean_radius + 4 * mean_square)
                self.assertAlmostEqual(solid(self, mesh), analytic, places=8)
            self.assertEqual(mesh["vertices"][:, 2].min(), 19)
            self.assertEqual(mesh["vertices"][:, 2].max(), 19.3)
            for z in (0.04, 0.13, 0.27):
                for (r0, z0), (r1, z1) in pairwise(PROFILE):
                    if z0 < z < z1:
                        radius = r0 + (r1 - r0) * (z - z0) / (z1 - z0)
                        break
                expected = polygon.buffer(radius, join_style=2).difference(polygon)
                actual = horizontal_slice(mesh, 19 + z)
                self.assertLess(actual.symmetric_difference(expected).area, 2e-6)
            self.assertTrue(
                all(p.is_valid and p.area > 0 for p in edge_footprints(plan))
            )

    def test_every_rectangle_edge_subset_caps_and_stable_ids(self):
        plan = BandPlan(shapely.box(0, 0, 12, 18), PROFILE)
        full = solid(self, band_surface(plan))
        singles = []
        for count in range(1, 5):
            for edges in combinations(range(4), count):
                mesh = band_surface(plan, edges)
                volume = solid(self, mesh)
                self.assertGreater(volume, 0)
                self.assertEqual(set(mesh["edge_ids"]), set(edges))
                if count == 1:
                    singles.append(volume)
        self.assertAlmostEqual(sum(singles), full, places=9)

    def test_scalar_batch_identical_empty_and_input_ownership(self):
        plan = BandPlan(shapely.box(0, 0, 12, 18), PROFILE)
        for edges in (None, (), (0, 2), (1, 2, 3)):
            a, b = band_surface(plan, edges), band_surface(plan, edges, method="scalar")
            for key in a:
                np.testing.assert_array_equal(a[key], b[key])
            self.assertEqual(a["faces"].shape[1], 3)
            self.assertFalse(np.shares_memory(a["vertices"], b["vertices"]))

    def test_capacity_precedes_expansion(self):
        plan = BandPlan(shapely.box(0, 0, 12, 18), PROFILE)
        with (
            patch(
                "outshine_ref.buildings.profiled_bands._quads",
                side_effect=AssertionError("expanded"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            band_surface(plan, max_triangles=1)

    def test_miter_and_courtyard_collapse_reject(self):
        acute = shapely.Polygon([(0, 0), (10, 0), (0.1, 0.1)])
        with self.assertRaisesRegex(ValueError, "miter"):
            band_surface(BandPlan(acute, PROFILE))
        hole = shapely.Polygon(
            [(0, 0), (20, 0), (20, 20), (0, 20)],
            [[(9.9, 9.9), (10.1, 9.9), (10.1, 10.1), (9.9, 10.1)]],
        )
        with self.assertRaisesRegex(ValueError, "reverses|topology"):
            band_surface(BandPlan(hole, PROFILE))

    def test_bad_plans_and_selection(self):
        polygon = shapely.box(0, 0, 12, 18)
        for profile in (
            list(PROFILE),
            ((0.0, 0.0), (0.0, 0.2), (0.2, 0.2)),
            ((0.0, 0.0), (0.2, 0.4), (0.2, 0.2), (0.0, 0.3)),
        ):
            with self.assertRaises(ValueError):
                BandPlan(polygon, profile)
        plan = BandPlan(polygon, PROFILE)
        for edges in ((0, 0), (True,), (-1,), (4,), [0]):
            with self.assertRaises(ValueError):
                band_surface(plan, edges)


if __name__ == "__main__":
    unittest.main()
