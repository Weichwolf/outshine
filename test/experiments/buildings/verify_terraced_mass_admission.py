"""Actual source failures and unchanged finite-actor/solid admission boundaries."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import shapely
from entrance_clearance import clearance
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from outshine_ref.buildings.modelling.prism_union import primitive_mesh
from terraced_admission_probe import controls
from terraced_facade_products import FacadeProducts
from terraced_mass_admission import closed, inspect, shell_sweep
from terraced_mass_frame import local_plan
from terraced_mass_geometry import bands, physical
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan


class AdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = controls()
        cls.owners = [
            TerraceProducts(plan(mass_plan(r, r["programme"]))) for r in cls.rows
        ]
        for owner in cls.owners:
            owner.facade.physical()

    def test_all_six_original_failures_pass_complete_independent_checks(self):
        self.assertEqual(len(self.owners), 6)
        for row, owner in zip(self.rows, self.owners, strict=True):
            with self.subTest(source=row["id"]):
                result = inspect(owner)
                self.assertGreater(len(result["terraces"]), 0)
                self.assertLessEqual(result["shell_volume_error_m3"], 1e-7)
                self.assertEqual(
                    owner.plan.prepared.mass.source_wkb, row["polygon"].wkb
                )

    def test_failed_raw_prisms_get_bounded_contacts_without_changing_logical_source(
        self,
    ):
        count = 0
        for row in self.rows:
            if row["original_failure"]["phase"] != "physical":
                continue
            mass = local_plan(mass_plan(row, row["programme"]))
            source = mass.source_wkb
            with self.assertRaisesRegex(ValueError, "Prism source rejected"):
                union_prisms(bands(mass))
            value = physical(mass)
            self.assertEqual(mass.source_wkb, source)
            self.assertAlmostEqual(
                value.volume_m3,
                sum(s.polygon.area * (s.high_m - s.low_m) for s in bands(mass)),
                delta=1e-7,
            )
            count += 1
        self.assertEqual(count, 3)

    def test_contact_resolution_rejects_a_materially_changed_profile(self):
        row = next(r for r in self.rows if r["original_failure"]["phase"] == "physical")
        mass = local_plan(mass_plan(row, row["programme"]))
        with (
            patch(
                "terraced_mass_geometry.shapely.set_precision",
                side_effect=lambda p, _grid: p.buffer(-0.01),
            ),
            self.assertRaisesRegex(ValueError, "1e-7 cubic metres"),
        ):
            physical(mass)

    def test_independent_volume_rejects_corrupted_saved_measurements(self):
        product = self.owners[0].facade.physical().floors.shell
        with self.assertRaisesRegex(ValueError, "volume changed"):
            closed(replace(product, volume_m3=product.volume_m3 + 0.01))

    def test_independent_sections_reject_an_unopened_actual_portal(self):
        owner = self.owners[0]
        previous = owner.facade
        owner.facade = FacadeProducts(owner.plan.prepared)
        try:
            with self.assertRaisesRegex(ValueError, "Independent shell section"):
                shell_sweep(owner)
        finally:
            owner.facade = previous


class ArrivalBoundaryTests(unittest.TestCase):
    def test_distant_inclined_component_does_not_change_arrival_and_near_one_is_rejected(
        self,
    ):
        points, faces = primitive_mesh(
            PrismVolume("floor", shapely.box(-2, -2, 2, 2), 0, 0.16)
        )
        floor = {"vertices": points, "faces": faces}
        vertices, triangles = primitive_mesh(
            PrismVolume("inclined", shapely.box(-0.1, -0.7, 0.1, -0.5), 0.6, 1.2)
        )
        vertices[vertices[:, 2] > 1, 0] += 0.2
        near = {"vertices": vertices, "faces": triangles}
        far = near | {"vertices": vertices + [10, 0, 0]}
        entry = {"door": {"centre": [0, 0], "tangent": [1, 0]}}
        baseline = clearance(entry, [floor])
        actual = clearance(entry, [floor, far])
        self.assertEqual(baseline["path_length_m"], actual["path_length_m"])
        with self.assertRaisesRegex(ValueError, "vertical prisms near the actor"):
            clearance(entry, [floor, near])


if __name__ == "__main__":
    unittest.main()
