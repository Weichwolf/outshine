"""Independent actual deck/guard/open-portal controls and rejected mutations."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
import terraced_terrace_frame as terrace_frame
import terraced_terrace_plan as terrace_planning
from building_layout import WALL_M, rectangle
from building_plan import FLOOR_M
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_union import primitive_mesh
from structural_shell_geometry import native
from terraced_facade_prepare import prepare
from terraced_facade_products import FacadeProducts
from terraced_mass_plan import plan as mass_plan
from terraced_mass_rooms import build
from terraced_mass_sources import sources
from terraced_terrace_access import (
    check,
    check_guards,
    continuous,
    selected_parts,
    world_path,
)
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan


class TerraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.owners = [
            TerraceProducts(plan(mass_plan(row, kind)))
            for row in sources()
            for kind in ("cascade", "paired-wings")
        ]
        for o in cls.owners:
            o.facade.physical()

    def test_original_decks_profiles_and_stable_replaced_window_ids(self):
        rejected = 0
        for o in self.owners:
            p, mass = o.plan, o.plan.prepared.mass
            self.assertEqual(len(set(p.replaced_window_ids)), len(p.portals))
            self.assertEqual(
                set(p.replaced_window_ids),
                {t.portal.replaced_window_id for t in p.terraces if t.portal},
            )
            self.assertFalse(
                set(p.replaced_window_ids)
                & {w.identifier for w in p.prepared.facade.windows}
            )
            for i, s in enumerate(mass.storeys):
                actual = shapely.union_all(
                    [shapely.from_wkb(t.deck_wkb) for t in p.terraces if t.storey == i]
                )
                expected = shapely.from_wkb(s.terrace_wkb)
                self.assertLess(actual.symmetric_difference(expected).area, 1e-7)
            for t in p.terraces:
                self.assertAlmostEqual(
                    t.floor_m, mass.storeys[t.storey].bottom_m + FLOOR_M
                )
                rejected += int(t.reason is not None)
                self.assertEqual(t.portal is None, t.reason is not None)
        self.assertGreater(rejected, 0)

    def test_all_added_components_are_closed_with_independent_volume_and_area(self):
        for o in self.owners:
            for c in o.components:
                s, mesh = c.source, c.mesh
                n = native(mesh)
                self.assertAlmostEqual(
                    n.volume(), s.polygon.area * (s.high_m - s.low_m), delta=1e-7
                )
                self.assertAlmostEqual(
                    n.surface_area(),
                    2 * s.polygon.area + s.polygon.length * (s.high_m - s.low_m),
                    delta=1e-7,
                )
                self.assertLessEqual(
                    float(mesh["vertices"][:, 2].max()), o.plan.prepared.mass.top_m
                )
                for a in mesh.values():
                    self.assertFalse(a.flags.writeable)
                    self.assertTrue(np.isfinite(a).all())

    def test_actual_portal_volume_and_prior_stair_arrival_components(self):
        for o in self.owners:
            mass = o.plan.prepared.mass
            previous, actual = build(mass), o.facade.physical()
            windows = sum(
                w.width_m * w.height_m for w in o.plan.prepared.facade.windows
            )
            portals = sum(p.width_m * p.height_m for p in o.plan.portals)
            self.assertAlmostEqual(
                previous.shell.volume_m3 - actual.floors.shell.volume_m3,
                WALL_M * (windows + portals),
                delta=1e-7,
            )
            for name in ("stairs", "landing"):
                for k, a in getattr(previous, name).mesh.items():
                    np.testing.assert_array_equal(
                        a, getattr(actual.floors, name).mesh[k]
                    )
            n = native(actual.floors.shell.mesh, actual.floors.shell.vertex_roots)
            self.assertAlmostEqual(
                n.volume(), actual.floors.shell.volume_m3, delta=1e-7
            )

    def test_continuous_actual_core_and_portal_routes_and_drop_protection(self):
        count = 0
        for o in self.owners:
            records = check(o)
            self.assertEqual(len(records), len(o.plan.portals))
            count += len(records)
        self.assertGreaterEqual(count, 16)

    def test_removed_deck_fails_continuous_foot_support(self):
        o = copy.copy(self.owners[0])
        missing = o.plan.terraces[0].identifier + "/deck"
        o.components = tuple(c for c in o.components if c.source.identifier != missing)
        with self.assertRaisesRegex(ValueError, "missing continuous foot support"):
            check(o)

    def test_unopened_wall_fails_the_actual_actor_tube(self):
        o = copy.copy(self.owners[0])
        o.facade = FacadeProducts(o.plan.prepared)
        with self.assertRaisesRegex(ValueError, "obstructs the continuous actor tube"):
            check(o)

    def test_removed_guard_fails_actual_original_drop_edge_coverage(self):
        o = copy.copy(self.owners[0])
        missing = o.plan.terraces[0].identifier + "/guard"
        o.components = tuple(c for c in o.components if c.source.identifier != missing)
        with self.assertRaisesRegex(ValueError, "edge protection"):
            check_guards(o)

    def test_four_centimetre_threshold_fails_continuous_lower_legs(self):
        o = self.owners[0]
        p = o.plan.portals[0]
        path = world_path(o.plan, p.path_wkb)
        centre = np.asarray(path.interpolate(0.85).coords[0])
        tangent = np.asarray(path.coords)[-1] - np.asarray(path.coords)[0]
        tangent /= np.linalg.norm(tangent)
        threshold = rectangle(centre, [tangent[1], -tangent[0]], 1.20, 0.04, 0.04)
        points, faces = primitive_mesh(
            PrismVolume("raised-counter", threshold, p.bottom_m, p.bottom_m + 0.04)
        )
        parts = selected_parts(o, path, p.bottom_m) + [
            {"vertices": points, "faces": faces}
        ]
        with self.assertRaisesRegex(ValueError, "obstructs the continuous actor tube"):
            continuous(parts, path, p.bottom_m)

    def test_direct_surface_construction_does_not_request_physical_floors(self):
        with patch(
            "terraced_facade_geometry.build",
            side_effect=AssertionError("unrequested physical floors"),
        ):
            for old in self.owners:
                o = TerraceProducts(old.plan)
                draw = o.draw(False)
                self.assertIsNone(o.facade.near)
                self.assertTrue(np.isfinite(draw["normals"]).all())
                self.assertEqual(draw["faces"].dtype, np.dtype(np.uint32))

    def test_empty_setbacks_do_not_repeat_profile_resolution(self):
        for old in self.owners:
            mass = old.plan.prepared.mass
            active = [
                s
                for i, s in enumerate(mass.storeys)
                if i and not shapely.from_wkb(s.terrace_wkb).is_empty
            ]
            with patch.object(
                terrace_planning, "resolved", wraps=terrace_planning.resolved
            ) as resolved:
                current = plan(mass)
            self.assertEqual(resolved.call_count, 2 * len(active))
            self.assertEqual(current.replaced_window_ids, old.plan.replaced_window_ids)

    def test_only_portal_storeys_transform_window_axes(self):
        for old in self.owners:
            mass = old.plan.prepared.mass
            prepared = prepare(mass)
            active = [
                i
                for i, s in enumerate(mass.storeys)
                if i and not shapely.from_wkb(s.terrace_wkb).is_empty
            ]
            with patch.object(
                terrace_frame, "world_facade", wraps=terrace_frame.world_facade
            ) as convert:
                world = terrace_frame.world_prepared(prepared, active)
            converted, cuts = convert.call_args.args
            self.assertEqual(
                len(cuts), sum(w.storey in active for w in prepared.facade.windows)
            )
            self.assertLess(len(cuts), len(prepared.facade.windows))
            self.assertTrue(all(w.storey in active for w in converted.facade.windows))
            self.assertEqual(len(world.facade.windows), len(cuts))


if __name__ == "__main__":
    unittest.main()
