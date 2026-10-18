"""Early trim omission preserves complete actual source openings and programmes."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import terraced_facade_details as detail_module
from bay_city_checks import inspect as inspect_layers
from bay_facade_controls import cases
from building_mesh import solid
from facade_programme_plan import PROGRAMMES, plan_programme
from facade_programme_products import (
    opening_frames,
    programme_products,
    without_trim_surface,
)
from terraced_crown_plan import plan as roof_plan
from terraced_facade_details import details
from terraced_facade_surface import compact
from terraced_mass_admission import inspect
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terrace_plan


def equal_arrays(before, after):
    if before.keys() != after.keys():
        raise AssertionError("Source product fields changed")
    for key in before:
        a, b = before[key], after[key]
        if a.dtype != b.dtype or a.shape != b.shape or a.tobytes() != b.tobytes():
            raise AssertionError("Source product changed: " + key)


def no_trim(storage, geometry, low, high, material):
    if material == 9:
        raise AssertionError("Unneeded old closed trim")
    return solid(storage, geometry, low, high, material)


class EarlyFacadeOmission(unittest.TestCase):
    def test_all_actual_closed_openings_and_compact_products_retain_exact_arrays(self):
        for row, height, terrace in cases():
            p = terrace.prepared
            with self.subTest(source=row["id"], height=height):
                original = details(p.local, p.facade, portals=terrace.portals)
                with patch.object(detail_module, "solid", side_effect=no_trim):
                    direct = details(
                        p.local, p.facade, portals=terrace.portals, legacy_trim=False
                    )
                self.assertNotIn(9, direct["materials"])
                self.assertEqual(original["faces"].dtype, direct["faces"].dtype)
                equal_arrays(opening_frames(original), opening_frames(direct))
                old = compact(p, portals=terrace.portals)
                with patch(
                    "terraced_facade_surface.accents",
                    side_effect=AssertionError("Unneeded old compact trim"),
                ):
                    new = compact(p, portals=terrace.portals, legacy_trim=False)
                equal_arrays(without_trim_surface(old.mesh), new.mesh)
                self.assertEqual(old.window_ids, new.window_ids)
                self.assertFalse(np.any(new.mesh["materials"] == 9))

    def test_complete_early_physical_programmes_and_all_draws_match_reference(self):
        rows = sources()
        for index, kind in ((0, "cascade"), (3, "paired-wings")):
            crown = roof_plan(rows[index], kind)
            terrace = terrace_plan(crown.body)
            for style in PROGRAMMES:
                with self.subTest(source=rows[index]["id"], style=style):
                    planned = plan_programme(
                        terrace.prepared, style, portals=terrace.portals
                    )
                    reference = programme_products(crown, planned, omit_trim=False)
                    with patch(
                        "terraced_facade_surface.accents",
                        side_effect=AssertionError("Unneeded old compact trim"),
                    ):
                        direct = programme_products(crown, planned)
                    self.assertFalse(direct.source.body.facade.legacy_trim)
                    before = reference.body.facade.physical()
                    with patch.object(detail_module, "solid", side_effect=no_trim):
                        after = direct.body.facade.physical()
                    equal_arrays(before.frames, after.frames)
                    for name in ("shell", "stairs", "landing"):
                        equal_arrays(
                            getattr(before.floors, name).mesh,
                            getattr(after.floors, name).mesh,
                        )
                    self.assertLessEqual(
                        inspect(direct.body)["maximum_shell_section_xor_m2"], 1e-7
                    )
                    self.assertTrue(inspect_layers(planned, terrace)["closed_layers"])
                    for near in (False, True):
                        for roof in (False, True):
                            equal_arrays(
                                reference.draw(body_near=near, roof_near=roof),
                                direct.draw(body_near=near, roof_near=roof),
                            )
                    chosen = tuple(c.identifier for c in planned.cells[:3])
                    for selected in ((), chosen):
                        equal_arrays(
                            reference.draw(body_near=False, selected_ids=selected),
                            direct.draw(body_near=False, selected_ids=selected),
                        )

    def test_empty_programme_retains_legacy_body_without_omitting_owned_trim(self):
        crown = roof_plan(sources()[0], "cascade")
        terrace = terrace_plan(crown.body)
        empty = plan_programme(
            terrace.prepared,
            PROGRAMMES[0],
            portals=terrace.portals,
            max_triangles=0,
        )
        self.assertFalse(empty.cells)
        direct = programme_products(crown, empty)
        self.assertTrue(direct.source.body.facade.legacy_trim)
        equal_arrays(
            direct.draw(body_near=False),
            programme_products(crown, empty, omit_trim=False).draw(body_near=False),
        )
        self.assertIn(9, direct.source.body.facade.coarse.mesh["materials"])
        with self.assertRaises(ValueError):
            programme_products(crown, empty, omit_trim=1)
        with (
            patch(
                "facade_programme_products.RoofedProducts",
                side_effect=AssertionError(
                    "Foreign source reached geometry construction"
                ),
            ),
            self.assertRaisesRegex(ValueError, "different source"),
        ):
            programme_products(crown, replace(empty, source_id="foreign"))


if __name__ == "__main__":
    unittest.main()
