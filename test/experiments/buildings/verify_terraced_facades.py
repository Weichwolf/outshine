"""Real source facade holes, immutable shared axes and unchanged actual access."""

import unittest

import numpy as np
import shapely

from building_layout import WALL_M
from entrance_clearance import clearance
from structural_shell_geometry import inspect_poses, native
from terraced_facade_geometry import build_facade
from terraced_mass_collision import ComponentColumns
from terraced_mass_plan import plan
from terraced_mass_rooms import build
from terraced_mass_sources import sources


class TerracedFacadeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.products = [
            (p, build(p), build_facade(p))
            for row in sources()
            for kind in ("cascade", "paired-wings")
            for p in [plan(row, kind)]
        ]

    def test_real_windows_have_stable_ids_axes_and_stay_in_storey_height(self):
        for p, _old, asset in self.products:
            windows = asset.facade.windows
            self.assertGreater(len(windows), 100)
            self.assertEqual(len({w.identifier for w in windows}), len(windows))
            anchor = np.asarray(shapely.from_wkb(p.source_wkb).centroid.coords[0])
            for w in windows:
                s = p.storeys[w.storey]
                self.assertGreater(w.bottom_m, s.bottom_m + 0.16)
                self.assertLess(w.bottom_m + w.height_m, s.top_m - 0.18)
                self.assertAlmostEqual(np.linalg.norm(w.tangent), 1.0)
                position = (np.asarray(w.centre) - anchor) @ np.asarray(w.tangent)
                self.assertAlmostEqual(
                    position, w.station * asset.facade.pitch_m, delta=1e-8
                )
                self.assertEqual(w.cutter_wkb, asset.cuts[windows.index(w)].polygon.wkb)
            self.assertEqual(asset.facade, build_facade(p).facade)

    def test_removed_volume_matches_independent_wall_thickness_times_window_areas(self):
        for _p, original, asset in self.products:
            removed = sum(w.width_m * w.height_m * WALL_M for w in asset.facade.windows)
            self.assertAlmostEqual(
                original.shell.volume_m3 - asset.floors.shell.volume_m3,
                removed,
                delta=1e-7,
            )

    def test_closed_shell_roundtrip_and_independent_triangle_volume(self):
        for p, _old, asset in self.products:
            shell = asset.floors.shell
            actual = native(shell.mesh, shell.vertex_roots)
            self.assertAlmostEqual(actual.volume(), shell.volume_m3, delta=1e-7)
            self.assertAlmostEqual(actual.surface_area(), shell.area_m2, delta=1e-7)
            xyz = shell.mesh["vertices"]
            triangles = xyz[shell.mesh["faces"]] - xyz.mean(axis=0)
            integral = (
                np.einsum(
                    "ij,ij->i",
                    triangles[:, 0],
                    np.cross(triangles[:, 1], triangles[:, 2]),
                ).sum()
                / 6
            )
            self.assertAlmostEqual(integral, shell.volume_m3, delta=1e-7)
            self.assertAlmostEqual(float(xyz[:, 2].min()), p.bottom_m)
            self.assertAlmostEqual(float(xyz[:, 2].max()), p.top_m)

    def test_every_actual_window_is_open_through_the_whole_wall(self):
        for _p, _old, asset in self.products:
            shell = asset.floors.shell
            actual = native(shell.mesh, shell.vertex_roots)
            for w in asset.facade.windows:
                tangent = np.asarray(w.tangent)
                normal = np.r_[tangent[1], -tangent[0], 0]
                centre = np.r_[w.centre, w.bottom_m + w.height_m / 2]
                for offset in (-0.35 * w.width_m, 0, 0.35 * w.width_m):
                    point = centre + np.r_[tangent * offset, 0]
                    self.assertEqual(
                        len(
                            actual.ray_cast(
                                point + normal * 0.04, point - normal * (WALL_M + 0.03)
                            )
                        ),
                        0,
                    )

    def test_actual_stairs_arrival_and_retained_parts_remain_valid(self):
        for p, old, asset in self.products:
            for name in ("stairs", "landing"):
                before, after = getattr(old, name), getattr(asset.floors, name)
                for key in before.mesh:
                    np.testing.assert_array_equal(before.mesh[key], after.mesh[key])
            poses = np.concatenate(
                [
                    np.asarray(p.core.path) + [0, 0, i * p.core.rise_m]
                    for i in range(len(p.storeys) - 1)
                ]
            )
            self.assertGreaterEqual(
                inspect_poses(ComponentColumns(asset.floors, -1, p.top_m + 1), poses),
                1.9,
            )
            clearance(
                {"door": {"centre": p.entry.centre, "tangent": p.entry.tangent}},
                [asset.floors.shell.mesh, asset.floors.landing.mesh],
            )

    def test_every_actual_frame_pane_and_accent_part_is_closed(self):
        for _p, _old, asset in self.products:
            mesh = asset.frames
            self.assertTrue(np.isfinite(mesh["vertices"]).all())
            self.assertTrue({3, 9, 10, 11}.issubset(set(map(int, mesh["materials"]))))
            for part in np.unique(mesh["parts"]):
                solid = native(mesh, rows=np.flatnonzero(mesh["parts"] == part))
                self.assertGreater(solid.volume(), 0)


if __name__ == "__main__":
    unittest.main()
