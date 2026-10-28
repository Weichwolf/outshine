"""Real guarded bays, preserved old routes and rejected physical counterexamples."""

import unittest
from copy import copy
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from facade_recess_checks import audit, component_geometry
from facade_recess_geometry import build, resolved
from facade_recess_plan import RecessRecipe, routes
from facade_recess_plan import plan as recess_plan
from facade_recess_shape import shape
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_union import primitive_mesh
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_facade_prepare import positions
from terraced_terrace_access import continuous, selected_parts, world_path
from terraced_terrace_plan import plan as terrace_plan


class RecessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = [plan(row, kind) for row, kind in cases()]
        cls.originals = [
            terrace_plan(p.body, routing="triangulated") for p in cls.plans
        ]
        cls.owners = [
            RoofedProducts(
                p, terrace_routing="triangulated", facade_recesses=RecessRecipe()
            )
            for p in cls.plans
        ]

    def test_all_actual_cells_keep_complete_sources_windows_and_former_portals(self):
        for original, owner, count in zip(
            self.originals, self.owners, (6, 4, 6), strict=True
        ):
            prepared = owner.body.plan.prepared
            self.assertEqual(prepared.mass, original.prepared.mass)
            self.assertEqual(prepared.local, original.prepared.local)
            self.assertEqual(owner.body.plan.portals, original.portals)
            self.assertEqual(len(prepared.recesses), count)
            old = {w.identifier: w for w in original.prepared.facade.windows}
            current = {w.identifier: w for w in prepared.facade.windows}
            selected = {r.source_window.identifier for r in prepared.recesses}
            self.assertEqual(set(old) - set(current), selected)
            for key, window in current.items():
                self.assertEqual(old[key], window)

    def test_actual_prisms_routes_slab_support_and_old_functional_checks(self):
        for original, owner in zip(self.originals, self.owners, strict=True):
            result = audit(owner.body, original)
            self.assertEqual(len(result["new_actor_routes"]), result["recesses"])
            self.assertTrue(
                all(len(r["routes"]) == 2 for r in result["new_actor_routes"])
            )
            self.assertGreater(result["existing_body"]["sampled_stair_poses"], 60)

    def test_unchanged_recess_geometry_is_shared_with_near_expansion(self):
        owner = RoofedProducts(self.plans[0], facade_recesses=RecessRecipe())
        facade = owner.body.facade
        self.assertIsNone(facade.near)
        cached = facade.recess_components
        with patch(
            "facade_recess_geometry.build",
            side_effect=AssertionError("duplicate assembly"),
        ):
            facade.physical()
        self.assertIs(cached, facade.recess_components)

    def test_missing_guard_cannot_pass_actual_component_validation(self):
        facade = copy(self.owners[0].body.facade)
        components = facade.recess_components
        guard = components.owners.index(components.descriptors[0].identifier + "/guard")
        keep = components.mesh["parts"] != guard
        mesh = {
            key: value if key == "vertices" else value[keep]
            for key, value in components.mesh.items()
        }
        facade.recess_components = replace(components, mesh=mesh)
        with self.assertRaisesRegex(ValueError, "Missing actual recess component"):
            component_geometry(facade)

    def test_closed_back_door_is_rejected_by_the_real_continuous_actor_tube(self):
        owner = self.owners[0].body
        recess = owner.plan.prepared.recesses[0]
        path = world_path(owner.plan, recess.door_route_wkb)
        obstruction = PrismVolume(
            "closed-door-control",
            shapely.from_wkb(recess.door.cutter_wkb),
            recess.door.bottom_m,
            recess.door.bottom_m + 1.9,
            12,
        )
        vertices, faces = primitive_mesh(obstruction)
        blocked = {
            "vertices": positions(vertices, owner.plan.prepared.mass.core),
            "faces": faces,
        }
        with self.assertRaisesRegex(ValueError, "continuous actor tube"):
            continuous(
                selected_parts(owner, path, recess.door.bottom_m) + [blocked],
                path,
                recess.door.bottom_m,
            )

    def test_cell_attempt_and_stack_budgets_preserve_unselected_windows(self):
        selection = recess_plan(
            self.originals[0], RecessRecipe(cells=1, stacks=1, attempts=1)
        )
        self.assertLessEqual(len(selection.cells), 1)
        self.assertEqual(dict(selection.work)["attempts"], 1)
        self.assertLessEqual(dict(selection.work)["accepted_stacks"], 1)

    def test_low_headers_and_source_escape_do_not_produce_closed_bays(self):
        prepared = self.owners[0].body.plan.prepared
        window = prepared.recesses[0].source_window
        mass = prepared.local
        storeys = list(mass.storeys)
        s = storeys[window.storey]
        storeys[window.storey] = replace(s, top_m=s.bottom_m + 2.25)
        with self.assertRaisesRegex(ValueError, "header_clearance"):
            shape(replace(mass, storeys=tuple(storeys)), window, RecessRecipe())
        moved = replace(window, centre=(10000.0, 10000.0))
        with self.assertRaisesRegex(ValueError, "pocket_leaves_source"):
            shape(mass, moved, RecessRecipe())

    def test_existing_declared_route_cannot_be_overwritten_by_a_new_pocket(self):
        prepared = self.owners[0].body.plan.prepared
        cell = prepared.recesses[0]
        protected = [shapely.from_wkb(cell.door_route_wkb)]
        with self.assertRaisesRegex(ValueError, "existing_actor_route"):
            routes(prepared.local, cell, RecessRecipe(), protected, (), {})

    def test_mismatched_assembly_and_repeated_source_ids_are_rejected(self):
        facade = self.owners[0].body.facade
        prepared, geometry = facade.prepared, facade.recess_components
        with self.assertRaisesRegex(ValueError, "different immutable facade plan"):
            resolved(replace(prepared, recesses=prepared.recesses[:-1]), geometry)
        with self.assertRaisesRegex(ValueError, "Duplicate recess"):
            build((prepared.recesses[0], prepared.recesses[0]))

    def test_triangle_budget_precedes_primitive_construction(self):
        with (
            patch("facade_recess_geometry.MAX_TRIANGLES", 1),
            patch(
                "facade_recess_geometry.primitive_mesh",
                side_effect=AssertionError("late budget"),
            ),
            self.assertRaisesRegex(ValueError, "input_triangle_budget"),
        ):
            build(self.owners[0].body.plan.prepared.recesses)

    def test_component_ids_remain_stable_for_individual_selected_cells(self):
        components = self.owners[0].body.facade.recess_components
        cell = components.descriptors[1]
        single = build((cell,))
        self.assertEqual(
            set(single.owners),
            {
                name
                for name in components.owners
                if name.startswith(cell.identifier + "/")
            },
        )

    def test_recipe_dimensions_and_capacity_domains_are_explicit(self):
        for change in (
            {"cells": True},
            {"cells": 25},
            {"depth_m": np.nan},
            {"depth_m": 0.2},
            {"door_height_m": 3},
            {"attempts": 97},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                RecessRecipe(**change)


if __name__ == "__main__":
    unittest.main()
