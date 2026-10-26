"""Actual null-topology roof/body counterexample and independent contour recovery."""

import unittest
import warnings
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.buildings.modelling import PrismVolume
from structural_shell_geometry import mesh_section
from terraced_admission_probe import controls
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import roof_checks
from terraced_mass_admission import inspect
from terraced_mass_geometry import bands
from terraced_mass_rooms import cavities


class CrownContact(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.row = controls(
            Path(__file__).with_name("data") / "terraced-crown-contact.json"
        )[0]
        with warnings.catch_warnings():
            warnings.simplefilter("error", RuntimeWarning)
            cls.owner = RoofedProducts(plan(cls.row, cls.row["programme"]))
            cls.checks = inspect(cls.owner.body)
            cls.roof_checks = roof_checks(cls.owner)
            cls.draw = cls.owner.draw(True)

    def test_complete_previous_physical_scope_and_finite_draw_preserve_source_height(
        self,
    ):
        mass = self.owner.plan.body
        self.assertEqual(len(mass.storeys), 9)
        self.assertEqual(self.owner.plan.source_top_m, 26)
        self.assertEqual(self.draw["vertices"][:, 2].max(), 26)
        self.assertGreater(self.checks["sampled_stair_poses"], 0)
        self.assertLess(self.checks["maximum_shell_section_xor_m2"], 1e-7)
        self.assertTrue(all(np.isfinite(v).all() for v in self.draw.values()))
        np.testing.assert_allclose(
            np.linalg.norm(self.draw["normals"], axis=1), 1, atol=1e-6, rtol=0
        )

    def test_null_interval_sections_follow_oriented_topology_and_actual_logical_cuts(
        self,
    ):
        body = self.owner.body
        shell = body.facade.near.floors.shell
        core = body.plan.prepared.mass.core
        vertices = shell.mesh["vertices"].copy()
        vertices[:, :2] = (vertices[:, :2] - core.origin_xy) @ np.array(
            [core.across_xy, core.along_xy]
        ).T
        actual = dict(shell.mesh, vertices=vertices)
        prepared = body.plan.prepared
        positive, cuts = bands(prepared.local), list(cavities(prepared.local))
        cuts.extend(
            PrismVolume(
                w.identifier,
                shapely.from_wkb(w.cutter_wkb),
                w.bottom_m,
                w.bottom_m + w.height_m,
            )
            for w in (*prepared.facade.windows, *body.plan.portals)
        )
        xyz = shell.mesh["vertices"][shell.mesh["faces"]]
        cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        heights = [6.64, 7.55]
        for triangle in xyz[np.all(cross == 0, axis=1)]:
            heights.extend((a + b) / 2 for a, b in pairwise(np.unique(triangle[:, 2])))
        for z in heights:
            expected = shapely.union_all(
                [p.polygon for p in positive if p.low_m < z < p.high_m]
            ).difference(
                shapely.union_all([p.polygon for p in cuts if p.low_m < z < p.high_m])
            )
            section = mesh_section(actual, shell.vertex_roots, z)
            self.assertLess(expected.symmetric_difference(section).area, 1e-7)


if __name__ == "__main__":
    unittest.main()
