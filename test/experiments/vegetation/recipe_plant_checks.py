"""Real conifer equivalence, demand pruning, spatial and actual precision negatives."""

import json
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from europe_conifer_recipes import conifers
from outshine_ref.contracts import LocalFrame
from outshine_ref.vegetation import Needle, NeedledPlant, StandHabitat
from recipe_graph import Prepared
from recipe_graph_data import Budget, plain
from recipe_plant_detail import physical_detail
from recipe_plant_operators import (
    FORM,
    RECIPES,
    SITE,
    operators,
    prepared,
)
from recipe_plant_sources import read_site, site_input, source_site
from recipe_plant_values import seed_input


def definitions():
    return {
        FORM: json.loads((RECIPES / "vegetation-pinus-young-form-v1.json").read_text()),
        SITE: json.loads((RECIPES / "vegetation-pinus-young-site-v1.json").read_text()),
    }


def loader(rows):
    return lambda key, remaining: json.dumps(rows[key]).encode()


class RecipePlantChecks(unittest.TestCase):
    def test_real_two_seed_forms_tables_prototypes_exact(self):
        species = next(s for s in conifers() if s.key == "pinus-sylvestris")
        graph = prepared()
        for seed in (718, 719):
            original = species.plant(seed)
            result = graph.evaluate({"seed": seed_input(seed)})
            self.assertEqual(plain(result.outputs["plant"].value), original.recipe())
            value = result.outputs["detail"].value
            self.assertEqual(physical_detail(value)["needles"], 4578)
            expected = original.tables(512, 16384)
            self.assertEqual(set(expected), set(value.tables))
            for k, a in expected.items():
                self.assertEqual(a.dtype, value.tables[k].dtype)
                np.testing.assert_array_equal(a, value.tables[k])
            for needle, actual in zip(original.needles, value.needles):
                for key, a in needle.mesh(8, 8).items():
                    self.assertEqual(a.dtype, actual[key].dtype)
                    np.testing.assert_array_equal(a, actual[key])

    def test_plan_only_cannot_build_needles_or_poses(self):
        with (
            patch.object(Needle, "mesh", side_effect=AssertionError),
            patch.object(NeedledPlant, "tables", side_effect=AssertionError),
        ):
            result = prepared().evaluate({"seed": seed_input(718)}, outputs=("plan",))
        self.assertEqual(result.stats["evaluated_nodes"], 1)
        self.assertEqual(result.stats["loaded_definitions"], 1)

    def test_seed_is_instance_data_and_definition_is_reused(self):
        graph = prepared()
        a = graph.evaluate({"seed": seed_input(718)}, outputs=("plant",))
        b = graph.evaluate({"seed": seed_input(719)}, outputs=("plant",))
        self.assertEqual(a.definition_fingerprint, b.definition_fingerprint)
        self.assertNotEqual(a.instance_fingerprint, b.instance_fingerprint)
        self.assertNotEqual(
            a.outputs["plant"].fingerprint, b.outputs["plant"].fingerprint
        )
        self.assertNotIn(
            '"seed": 718', (RECIPES / "vegetation-pinus-young-form-v1.json").read_text()
        )

    def test_real_root_placement_preserves_site_and_has_no_detail_work(self):
        site = source_site()
        with (
            patch.object(Needle, "mesh", side_effect=AssertionError),
            patch.object(NeedledPlant, "tables", side_effect=AssertionError),
        ):
            r = prepared(SITE).evaluate(
                {"seed": seed_input(718), "site": site_input(site)},
                outputs=("placement",),
            )
        self.assertEqual(r.stats["evaluated_nodes"], 5)
        np.testing.assert_array_equal(
            r.outputs["placement"].value["origin_m"], site.origin
        )
        self.assertEqual(r.outputs["placement"].value["site_id"], site.site_id)

    def test_actual_support_change_invalidates_site_but_not_form(self):
        site = source_site()
        h = site.habitat
        support = h.domain.difference(shapely.Point(site.origin[:2]).buffer(0.5))
        changed = replace(
            site,
            habitat=StandHabitat.prepare(
                h.frame,
                h.source_id,
                h.region_id,
                h.source_geometry,
                h.lower,
                h.side_m,
                support=support,
            ),
        )
        graph = prepared(SITE)
        before = graph.evaluate(
            {"seed": seed_input(718), "site": site_input(site)}, outputs=("plant",)
        )
        after = graph.evaluate(
            {"seed": seed_input(718), "site": site_input(changed)}, outputs=("plant",)
        )
        self.assertNotEqual(before.instance_fingerprint, after.instance_fingerprint)
        self.assertEqual(
            before.outputs["plant"].fingerprint, after.outputs["plant"].fingerprint
        )
        with (
            patch.object(Needle, "mesh", side_effect=AssertionError),
            self.assertRaisesRegex(ValueError, "no prepared habitat"),
        ):
            graph.evaluate(
                {"seed": seed_input(718), "site": site_input(changed)},
                outputs=("placement",),
            )

    def test_source_frame_units_datum_and_actual_float32_site_are_rejected(self):
        site = source_site()
        with self.assertRaisesRegex(ValueError, "source/frame"):
            read_site(
                site_input(
                    replace(
                        site, habitat=replace(site.habitat, frame=LocalFrame("foreign"))
                    )
                )
            )
        with self.assertRaisesRegex(ValueError, "Float64"):
            read_site(site_input(replace(site, origin=site.origin.astype(np.float32))))
        good = site_input(site)
        for key, value in (
            ("units", "cm"),
            ("height_datum", "measured-DEM"),
            ("frame_id", "foreign"),
        ):
            bad = replace(good, metadata=dict(good.metadata) | {key: value})
            with self.assertRaisesRegex(ValueError, "metadata"):
                read_site(bad)

    def test_detail_reader_checks_actual_float64_and_real_contacts(self):
        value = (
            prepared()
            .evaluate({"seed": seed_input(718)}, outputs=("detail",))
            .outputs["detail"]
            .value
        )
        altered = dict(value.tables)
        altered["foliage_origins"] = altered["foliage_origins"].astype(np.float32)
        with self.assertRaisesRegex(ValueError, "Float64"):
            physical_detail(replace(value, tables=altered))
        altered = dict(value.tables)
        altered["foliage_origins"] = altered["foliage_origins"].copy()
        altered["foliage_origins"][0, 0] += 0.01
        with self.assertRaisesRegex(ValueError, "contacts"):
            physical_detail(replace(value, tables=altered))

    def test_emissions_and_external_geometry_are_really_read_only(self):
        site = source_site()
        with self.assertRaises(ValueError):
            site.origin.setflags(write=True)
        result = prepared().evaluate({"seed": seed_input(718)}, outputs=("detail",))
        detail = result.outputs["detail"].value
        for a in (detail.tables["foliage_origins"], detail.needles[0]["vertices"]):
            with self.assertRaises(ValueError):
                a.setflags(write=True)

    def test_unphysical_needled_twigs_are_refused_before_meshing(self):
        rows = definitions()
        wood = rows[FORM]["nodes"]["wood"]["parameters"]["levels"][1]
        wood.update(base_radius=0.5, tip_radius=0.4)
        with (
            patch.object(Needle, "mesh", side_effect=AssertionError),
            self.assertRaises(ValueError),
        ):
            prepared(loader=loader(rows)).evaluate(
                {"seed": seed_input(718)}, outputs=("detail",)
            )

    def test_missing_version_cycles_and_capabilities_fail_before_operators(self):
        rows = definitions()
        rows[SITE]["dependencies"]["form"]["version"] = "2"
        with self.assertRaises(KeyError):
            prepared(SITE, loader=loader(rows))
        rows = definitions()
        rows[FORM]["dependencies"]["self"] = {
            "id": FORM.id,
            "version": "1",
            "inputs": {"seed": "seed"},
        }
        with self.assertRaisesRegex(ValueError, "Cyclic"):
            prepared(loader=loader(rows))
        with self.assertRaisesRegex(ValueError, "capabilities"):
            Prepared(
                FORM,
                loader(definitions()),
                operators(),
                capabilities=(),
                budget=Budget(),
            )

    def test_input_type_and_reservation_fail_before_operators(self):
        with patch("recipe_plant_operators.wood", side_effect=AssertionError):
            graph = prepared(budget=Budget(max_product_bytes=1))
            with self.assertRaisesRegex(ValueError, "reservation"):
                graph.evaluate({"seed": seed_input(718)}, outputs=("plan",))
            with self.assertRaisesRegex(ValueError, "types"):
                prepared().evaluate(
                    {"seed": site_input(source_site())}, outputs=("plan",)
                )

    def test_dangling_node_and_detail_work_budget_fail_before_mesh_generation(self):
        rows = definitions()
        rows[FORM]["nodes"]["plant"]["inputs"]["variants"]["node"] = "absent"
        with self.assertRaises(ValueError):
            prepared(loader=loader(rows))
        rows = definitions()
        rows[FORM]["nodes"]["detail"]["parameters"]["max_needles"] = 1
        with (
            patch.object(Needle, "mesh", side_effect=AssertionError),
            patch.object(NeedledPlant, "tables", side_effect=AssertionError),
            self.assertRaisesRegex(ValueError, "before placement"),
        ):
            prepared(loader=loader(rows)).evaluate(
                {"seed": seed_input(718)}, outputs=("detail",)
            )

    def test_definition_change_invalidates_actual_dependents(self):
        rows = definitions()
        first = prepared(loader=loader(rows)).evaluate(
            {"seed": seed_input(718)}, outputs=("plant",)
        )
        rows[FORM]["nodes"]["variants"]["parameters"]["needles"][0]["length"] *= 1.1
        second = prepared(loader=loader(rows)).evaluate(
            {"seed": seed_input(718)}, outputs=("plant",)
        )
        self.assertNotEqual(first.definition_fingerprint, second.definition_fingerprint)
        self.assertNotEqual(
            first.outputs["plant"].fingerprint, second.outputs["plant"].fingerprint
        )


if __name__ == "__main__":
    unittest.main()
