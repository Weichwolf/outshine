"""Independent shell sections, serialized solids and sampled actual stair access."""

from itertools import pairwise
import math
import unittest

import numpy as np
import shapely

from building_plan import FLOOR_M
from entrance_clearance import clearance
from structural_shell_geometry import inspect_poses, mesh_section, native
from terraced_mass_collision import ComponentColumns
from terraced_mass_frame import local_plan, placed
from terraced_mass_geometry import bands
from terraced_mass_plan import plan
from terraced_mass_rooms import build
from terraced_mass_sources import sources


def expected_section(p, cuts, z):
    positive = shapely.union_all(
        [b.polygon for b in bands(p) if b.low_m < z < b.high_m]
    )
    negative = shapely.union_all([c.polygon for c in cuts if c.low_m < z < c.high_m])
    return positive.difference(negative)


class OpenFloorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.products = [
            (p, build(p))
            for row in sources()
            for kind in ("cascade", "paired-wings")
            for p in [plan(row, kind)]
        ]

    def test_shell_volume_and_sections_follow_independent_2d_sweep(self):
        for p, asset in self.products:
            # Independent sections in the same explicit metric scope avoid GEOS's
            # world-coordinate near-coincident polygon-overlay failure.
            local = local_plan(p)
            local_asset = build(local)
            for name in ("shell", "stairs", "landing"):
                projected = placed(getattr(local_asset, name), p.core)
                world = getattr(asset, name)
                np.testing.assert_array_equal(
                    projected.mesh["faces"], world.mesh["faces"]
                )
                np.testing.assert_allclose(
                    projected.mesh["vertices"],
                    world.mesh["vertices"],
                    rtol=0,
                    atol=1e-10,
                )
            heights = sorted(
                {v for b in (*bands(p), *asset.cuts) for v in (b.low_m, b.high_m)}
            )
            volume = 0.0
            for lo, hi in pairwise(heights):
                z = (lo + hi) / 2
                expected = expected_section(local, local_asset.cuts, z)
                volume += expected.area * (hi - lo)
                actual = mesh_section(
                    local_asset.shell.mesh, local_asset.shell.vertex_roots, z
                )
                self.assertLessEqual(expected.symmetric_difference(actual).area, 1e-7)
            self.assertAlmostEqual(asset.shell.volume_m3, volume, delta=1e-7)

    def test_closed_products_survive_serialization_without_changed_volume_or_area(self):
        for _p, asset in self.products:
            for product in (asset.shell, asset.stairs, asset.landing):
                solid = native(product.mesh, product.vertex_roots)
                self.assertAlmostEqual(solid.volume(), product.volume_m3, delta=1e-7)
                self.assertAlmostEqual(
                    solid.surface_area(), product.area_m2, delta=1e-7
                )
                points = np.asarray(product.mesh["vertices"], dtype=np.float64)
                triangles = points[np.asarray(product.mesh["faces"])] - points.mean(
                    axis=0
                )
                volume = (
                    np.einsum(
                        "ij,ij->i",
                        triangles[:, 0],
                        np.cross(triangles[:, 1], triangles[:, 2]),
                    ).sum()
                    / 6
                )
                self.assertAlmostEqual(volume, product.volume_m3, delta=1e-7)

    def test_actual_stair_tread_samples_have_support_and_clearance_on_whole_asset(self):
        for p, asset in self.products:
            poses = np.concatenate(
                [
                    np.asarray(p.core.path) + [0, 0, i * p.core.rise_m]
                    for i in range(len(p.storeys) - 1)
                ]
            )
            clearance_m = inspect_poses(ComponentColumns(asset, -1, p.top_m + 1), poses)
            self.assertGreaterEqual(clearance_m, 1.90)

    def test_continuous_ground_arrival_and_connected_approach(self):
        for p, asset in self.products:
            arrival = clearance(
                {"door": {"centre": p.entry.centre, "tangent": p.entry.tangent}},
                [asset.shell.mesh, asset.landing.mesh],
            )
            self.assertAlmostEqual(arrival["path_length_m"], 2.3)
            route = shapely.from_wkb(p.core.approach_wkb)
            factor = 1 / math.cos(math.pi / (4 * 32))
            tube = route.buffer(0.30 * factor, quad_segs=32)
            stair_hull = shapely.MultiPoint(
                asset.stairs.mesh["vertices"][:, :2]
            ).convex_hull
            self.assertLessEqual(tube.intersection(stair_hull).area, 1e-8)
            for start, end in pairwise(np.asarray(route.coords)):
                delta = end - start
                length = np.linalg.norm(delta)
                direction = delta / length
                clearance(
                    {
                        "door": {
                            "centre": (start + end) / 2,
                            "tangent": [-direction[1], direction[0]],
                        }
                    },
                    [asset.shell.mesh, asset.landing.mesh],
                    outside_m=length / 2,
                    inside_m=length / 2,
                )
            distances = np.linspace(
                0, route.length, int(np.ceil(route.length / 0.10)) + 1
            )
            points = shapely.get_coordinates(
                shapely.line_interpolate_point(route, distances)
            )
            poses = np.c_[points, np.full(len(points), p.bottom_m + FLOOR_M)]
            self.assertGreaterEqual(
                inspect_poses(ComponentColumns(asset, -1, p.top_m + 1), poses), 1.90
            )


if __name__ == "__main__":
    unittest.main()
