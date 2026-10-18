"""Independent real-source volume, exterior-contact and access-height controls."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import manifold3d
import numpy as np
import shapely
from building_layout import rectangle
from building_plan import FLOOR_M
from structural_shell_geometry import native
from terraced_crown_plan import plan as crowned_plan
from terraced_facade_prepare import prepare
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from window_shade_mesh import expand
from window_shade_plan import PROGRAMMES, conflict, local_slot, plan_shades


class WindowShadeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = [
            prepare(mass)
            for row in sources()
            for kind in ("cascade", "paired-wings")
            for mass in (plan(row, kind), crowned_plan(row, kind).body)
        ]
        cls.plans = [
            (p, plan_shades(p, style)) for p in cls.prepared for style in PROGRAMMES
        ]

    def test_immutable_source_and_stable_ids_without_any_body_expansion(self):
        for prepared, actual in self.plans:
            with patch(
                "window_shade_mesh.template", side_effect=AssertionError("mesh")
            ):
                reversed_plan = plan_shades(
                    replace(
                        prepared,
                        facade=replace(
                            prepared.facade,
                            windows=tuple(reversed(prepared.facade.windows)),
                        ),
                    ),
                    actual.programme,
                )
            self.assertEqual(
                {s.identifier: s for s in actual.slots},
                {s.identifier: s for s in reversed_plan.slots},
            )
            self.assertEqual(prepared, prepare(prepared.mass))
            self.assertEqual(
                len(actual.slots) + len(actual.rejected), len(prepared.facade.windows)
            )
            self.assertGreater(len(actual.slots), 0)

    def test_actual_vertices_have_full_headroom_and_only_the_declared_wall_contact(
        self,
    ):
        for prepared, planned in self.plans:
            mesh = expand(planned)
            points = mesh["vertices"][mesh["faces"]]
            for index, slot in enumerate(planned.slots):
                xyz = points[mesh["slot_indices"] == index].reshape(-1, 3)
                floor = prepared.mass.storeys[slot.storey]
                self.assertGreaterEqual(
                    xyz[:, 2].min(), floor.bottom_m + FLOOR_M + 2.10 - 1e-10
                )
                self.assertLessEqual(xyz[:, 2].max(), floor.top_m - 0.06 + 1e-10)
                centre, tangent = np.asarray(slot.centre), np.asarray(slot.tangent)
                relative = xyz[:, :2] - centre
                inward = np.array([-tangent[1], tangent[0]])
                self.assertAlmostEqual(
                    float((relative @ tangent).min()), -slot.width_m / 2, delta=1e-8
                )
                self.assertAlmostEqual(
                    float((relative @ inward).min()), -slot.depth_m, delta=1e-8
                )
                self.assertAlmostEqual(
                    float((relative @ inward).max()), slot.back_m, delta=1e-8
                )
                projection = shapely.MultiPoint(xyz[:, :2]).convex_hull
                attachment = rectangle(
                    slot.centre, slot.tangent, slot.width_m, 0, slot.back_m
                )
                host = shapely.from_wkb(floor.footprint_wkb)
                self.assertLess(
                    projection.difference(attachment).intersection(host).area, 1e-8
                )
                self.assertTrue(host.buffer(1e-8).covers(attachment))

    def test_closed_positive_solids_match_analytic_volume_and_independent_bar_union(
        self,
    ):
        for _prepared, planned in self.plans:
            for slot in (
                planned.slots[0],
                planned.slots[len(planned.slots) // 2],
                planned.slots[-1],
            ):
                one = replace(planned, slots=(slot,))
                profile = expand(one)
                actual = native(profile)
                area = slot.width_m * (slot.top_m - slot.cap_bottom_m)
                if slot.programme == "returned-hood":
                    area += 2 * slot.thickness_m * (slot.cap_bottom_m - slot.bottom_m)
                expected = area * (slot.depth_m + slot.back_m)
                self.assertAlmostEqual(actual.volume(), expected, delta=1e-8)
                bars = expand(one, method="bars")
                solids = [
                    native(bars, rows=np.flatnonzero(bars["parts"] == p))
                    for p in np.unique(bars["parts"])
                ]
                union = manifold3d.Manifold.batch_boolean(solids, manifold3d.OpType.Add)
                self.assertAlmostEqual((actual - union).volume(), 0, delta=1e-8)
                self.assertAlmostEqual((union - actual).volume(), 0, delta=1e-8)

    def test_selection_precedes_geometry_and_reuses_identical_templates(self):
        p = self.plans[1][1]
        with patch("window_shade_mesh.template", side_effect=AssertionError("unused")):
            empty = expand(p, [])
            self.assertEqual(empty["vertices"].shape, (0, 3))
            self.assertEqual(empty["faces"].shape, (0, 3))
            with self.assertRaises(ValueError):
                expand(p, ["foreign"])
        ids = [s.identifier for s in p.slots[:3]]
        reused = expand(p, ids)
        fresh = expand(p, ids, reuse=False)
        for key in reused:
            np.testing.assert_array_equal(reused[key], fresh[key])
        self.assertEqual(set(reused["slot_indices"]), {0, 1, 2})
        full = expand(p)
        selected = full["slot_indices"] < 3
        np.testing.assert_array_equal(
            reused["vertices"][reused["faces"]],
            full["vertices"][full["faces"]][selected],
        )

    def test_bad_budget_and_duplicate_source_ids_reject_before_expansion(self):
        p = self.prepared[0]
        for value in (False, 1.0, float("nan"), float("inf"), 0, -1):
            with self.assertRaises(ValueError):
                plan_shades(p, "eyebrow", max_candidates=value)
        with self.assertRaises(ValueError):
            plan_shades(p, "eyebrow", max_candidates=len(p.facade.windows) - 1)
        with self.assertRaises(ValueError):
            plan_shades(p, "unknown")
        with self.assertRaises(ValueError):
            plan_shades(
                replace(p, facade=replace(p.facade, windows=p.facade.windows[:1] * 2)),
                "eyebrow",
            )

    def test_low_high_and_opposing_wall_counterexamples_are_rejected(self):
        p = self.prepared[0]
        w = p.facade.windows[0]
        floor = p.local.storeys[w.storey]
        slot = local_slot(w, floor, "returned-hood")
        self.assertIsNone(conflict(slot, p.local))
        self.assertIsNotNone(
            conflict(replace(slot, bottom_m=floor.bottom_m + FLOOR_M + 1.89), p.local)
        )
        self.assertIsNotNone(conflict(replace(slot, top_m=floor.top_m + 0.01), p.local))
        outward = rectangle(
            slot.centre, slot.tangent, slot.width_m, slot.depth_m, -0.03
        )
        obstructing = replace(floor, footprint_wkb=outward.wkb)
        occupied = replace(p.local, storeys=(*p.local.storeys, obstructing))
        self.assertIn("intersecting", conflict(slot, occupied))


if __name__ == "__main__":
    unittest.main()
