"""Whole-wall hierarchy, actual functional bodies and replacement-only component scope."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from bay_city_checks import inspect as inspect_layers
from bay_facade_controls import cases
from bay_facade_mesh import expand
from bay_wall_domains import domains
from clad_facade_mesh import surface
from facade_programme_plan import PROGRAMMES, plan_programme
from facade_programme_products import ProgrammeProducts
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import RoofedPlan
from terraced_crown_plan import plan as roof_plan
from terraced_mass_admission import inspect
from terraced_mass_census import source_identity


def owner(row, label, terrace):
    mass = terrace.prepared.mass
    plan = (
        roof_plan(row, mass.kind)
        if label == "roof-coupled"
        else RoofedPlan(mass, None, mass.top_m, 0, "Original-height control")
    )
    return RoofedProducts(plan)


class FacadeProgrammeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = cases()
        cls.plans = [
            (
                row,
                label,
                terrace,
                plan_programme(terrace.prepared, style, portals=terrace.portals),
            )
            for row, label, terrace in cls.cases
            for style in PROGRAMMES
        ]

    def test_hierarchy_reordering_capacity_and_blank_walls(self):
        for _row, _label, terrace, planned in self.plans:
            with self.subTest(source=planned.source_id, style=planned.programme):
                self.assertTrue(planned.cells)
                roles = {c.role for c in planned.cells}
                self.assertTrue({"plinth", "main", "attic", "field"} <= roles)
                shuffled = replace(
                    terrace.prepared,
                    facade=replace(
                        terrace.prepared.facade,
                        windows=tuple(reversed(terrace.prepared.facade.windows)),
                    ),
                )
                self.assertEqual(
                    planned,
                    plan_programme(
                        shuffled,
                        planned.programme,
                        portals=tuple(reversed(terrace.portals)),
                    ),
                )
                empty = plan_programme(
                    terrace.prepared,
                    planned.programme,
                    portals=terrace.portals,
                    max_triangles=0,
                )
                self.assertFalse(empty.cells)
                self.assertTrue(empty.rejected)
        terrace = self.cases[1][2]
        blank = replace(
            terrace.prepared, facade=replace(terrace.prepared.facade, windows=())
        )
        self.assertFalse(domains(blank)[0])
        self.assertTrue(domains(blank, include_blank=True)[0])
        self.assertTrue(plan_programme(blank, PROGRAMMES[0]).cells)
        with self.assertRaises(ValueError):
            domains(blank, include_blank=1)

    def test_all_actual_programmes_keep_complete_previous_function_gates(self):
        for row, label, terrace in self.cases:
            original = source_identity(row)
            source = owner(row, label, terrace)
            before = source.body.facade.physical()
            kept = before.frames["materials"] != 9
            original_triangles = before.frames["vertices"][before.frames["faces"]][kept]
            for style in PROGRAMMES:
                with self.subTest(source=row["id"], height=label, style=style):
                    planned = plan_programme(
                        terrace.prepared, style, portals=terrace.portals
                    )
                    product = ProgrammeProducts(source, planned)
                    physical = product.body.facade.physical()
                    self.assertIs(physical.floors, before.floors)
                    np.testing.assert_array_equal(
                        physical.frames["vertices"][physical.frames["faces"]][
                            : len(original_triangles)
                        ],
                        original_triangles,
                    )
                    np.testing.assert_array_equal(
                        physical.frames["parts"][: int(kept.sum())],
                        before.frames["parts"][kept],
                    )
                    self.assertNotIn(9, physical.frames["materials"])
                    self.assertEqual(original, source_identity(row))
                    inspect_layers(planned, terrace)
                    result = inspect(product.body)
                    self.assertLessEqual(result["maximum_shell_section_xor_m2"], 1e-7)
                    mesh = product.draw()
                    self.assertTrue(all(np.isfinite(a).all() for a in mesh.values()))
                    np.testing.assert_array_equal(
                        [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
                        [source.plan.body.bottom_m, source.plan.source_top_m],
                    )
                    self.assertIs(source.body.facade.physical(), before)

    def test_compact_and_empty_selection_do_not_build_near_or_hidden_sides(self):
        row, label, terrace = self.cases[1]
        source = owner(row, label, terrace)
        planned = plan_programme(
            terrace.prepared, PROGRAMMES[1], portals=terrace.portals
        )
        product = ProgrammeProducts(source, planned)
        with (
            patch.object(
                source.body.facade,
                "physical",
                side_effect=AssertionError("Unrequested near body"),
            ),
            patch(
                "clad_facade_mesh.sides",
                side_effect=AssertionError("Hidden profile sides"),
            ),
        ):
            mesh = product.draw(body_near=False, roof_near=False)
            self.assertGreater(len(mesh["faces"]), 0)
            self.assertIsNone(source.body.facade.near)
            self.assertIsNone(source.roof_components)
            with patch(
                "bay_facade_mesh.surface",
                side_effect=AssertionError("Unselected component"),
            ):
                product.draw(body_near=False, roof_near=False, selected_ids=())
            with self.assertRaisesRegex(ValueError, "foreign"):
                product.draw(selected_ids={"foreign"})

    def test_direct_products_match_closed_reference_geometry_normals_and_uvs(self):
        for _row, _label, _terrace, planned in self.plans:
            for cell in (
                planned.cells[0],
                planned.cells[len(planned.cells) // 2],
                planned.cells[-1],
            ):
                for layer in cell.layers:
                    full = surface(layer, "closed")
                    xyz = full["vertices"][full["faces"]]
                    for mode, keep in (
                        ("front", np.all(xyz[:, :, 2] == layer.depth_m, axis=1)),
                        ("outer", ~np.all(xyz[:, :, 2] == 0, axis=1)),
                    ):
                        direct = surface(layer, mode)
                        for key in ("vertices", "normals", "uv"):
                            np.testing.assert_array_equal(
                                full[key][full["faces"]][keep],
                                direct[key][direct["faces"]],
                            )
            with self.assertRaises(ValueError):
                expand(planned, {"foreign"})

    def test_shifted_opening_and_bad_capacity_are_rejected_without_rewriting_source(
        self,
    ):
        row, _label, terrace = self.cases[1]
        original = source_identity(row)
        window = terrace.prepared.facade.windows[0]
        shifted = replace(
            window,
            centre=tuple(
                np.asarray(window.centre)
                + 0.3 * np.array([window.tangent[1], -window.tangent[0]])
            ),
        )
        bad = replace(
            terrace.prepared,
            facade=replace(
                terrace.prepared.facade,
                windows=(shifted, *terrace.prepared.facade.windows[1:]),
            ),
        )
        with self.assertRaises(ValueError):
            plan_programme(bad, PROGRAMMES[0], portals=terrace.portals)
        for capacity in (-1, False, 1.5):
            with self.assertRaises(ValueError):
                plan_programme(terrace.prepared, PROGRAMMES[0], max_triangles=capacity)
        self.assertEqual(original, source_identity(row))


if __name__ == "__main__":
    unittest.main()
