"""Seven original parts, two actual stair bodies and provisional exterior caps."""

import copy
import unittest
from pathlib import Path

import numpy as np
import shapely
from mvt_t_compact_caps import compact_caps
from mvt_t_connection_inputs import inputs
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_region import region_inputs
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links

FIXTURE = Path(__file__).with_name("fixtures") / "mvt_stair_boundary_neighbours.json"


class SevenSourceCompactCaps(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture, _sources = inputs(FIXTURE)
        selection = [f"native-part:{w['native_way']}" for w in cls.capture["ways"]]
        cls.case, cls.sources, proposal = region_inputs(cls.capture, selection)
        cls.proposal = compact_caps(cls.sources, proposal, ["native-part:239"])
        cls.plan = prepare_profiles(
            cls.case, cls.sources, cls.proposal, mesh_recipe="outward_binary_grid"
        )
        cls.heights, cls.solve = solve_profiles(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.solve)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)
        cls.products = shared_products(cls.plan, cls.heights, cls.data)

    def test_both_saved_stairs_and_original_sources_pass_whole_body_audits(self):
        proof = audit_profiles(self.plan, self.heights, self.data)
        vertex_links(self.body.vertices, self.body.triangles)
        self.assertEqual(len(proof["body"]["source_coverage_error_m2"]), 7)
        self.assertLessEqual(max(proof["body"]["source_coverage_error_m2"]), 1e-7)
        self.assertEqual(len(self.geometry["stairs"]), 2)
        short = next(s for s in self.geometry["stairs"] if s["owner"] == 3)
        self.assertEqual(short["risers"], 1)
        self.assertLessEqual(short["rise_m"], 0.17 + 1e-7)
        self.assertEqual(self.geometry["float32_edge_cleanup"]["accepted_edges"], 1)

    def test_actual_run_capacity_is_bound_before_height_fit(self):
        recipe = self.plan["compact_flights"][0]
        self.assertEqual(recipe["maximum_risers"], 1)
        self.assertGreater(recipe["usable_run_m"], recipe["minimum_tread_depth_m"])
        self.assertLess(recipe["usable_run_m"], 2 * recipe["minimum_tread_depth_m"])
        self.assertEqual(recipe["maximum_rise_m"], 0.17)
        unsupported = shapely.box(0, 0, recipe["usable_run_m"] / 2, 1.5)
        self.assertTrue(unsupported.buffer(-0.14).is_empty)

    def test_independent_ipm_and_explicit_model_keep_the_same_quality_and_objective(
        self,
    ):
        for options in ({"solver": "highs-ipm"}, {"height_formulation": "equalities"}):
            with self.subTest(options=options):
                heights, note = solve_profiles(self.plan, **options)
                self.assertIsNotNone(heights, note)
                self.assertAlmostEqual(
                    note["objective"], self.solve["objective"], places=7
                )
                body, data, _geometry = build(self.plan, heights)
                audit_profiles(self.plan, heights, data)
                vertex_links(body.vertices, body.triangles)

    def test_four_unknown_boundaries_include_the_unbound_short_stair_end(self):
        ports = self.products["external_contacts"]
        self.assertEqual(len(ports), 4)
        self.assertTrue(
            all(p["boundary_role"] == "unknown_neighbour_not_free_end" for p in ports)
        )
        short = next(p for p in ports if p["source_id"] == "native-part:239")
        self.assertEqual(short["endpoint"], 0)
        self.assertFalse(any(p["source_id"] == "native-part:535" for p in ports))
        ranges = self.products["navigation"]["stair_level_ranges"]
        self.assertEqual(
            {r["source_id"] for r in ranges}, {"native-part:240", "native-part:239"}
        )
        self.assertEqual(ranges[-1]["end"], len(self.data["levels_m"]))

    def test_compact_preparation_keeps_capture_and_proposal_ownership(self):
        selection = [s["source_id"] for s in self.sources]
        _case, sources, proposal = region_inputs(self.capture, selection)
        before = copy.deepcopy(proposal)
        result = compact_caps(sources, proposal, ["native-part:239"])
        self.assertEqual(proposal, before)
        self.assertEqual(result["region_context"]["capture"], self.capture)
        self.assertEqual(result["region_context"]["unselected_sources"], [])

    def test_outside_neighbour_is_not_relabelled_as_an_unbound_cap(self):
        selected = [
            s["source_id"] for s in self.sources if s["source_id"] != "native-part:535"
        ]
        _case, sources, proposal = region_inputs(self.capture, selected)
        with self.assertRaisesRegex(ValueError, "outside or conflicting"):
            compact_caps(sources, proposal, ["native-part:239"])

    def test_t_junction_does_not_take_a_near_collinear_compact_recipe(self):
        with self.assertRaisesRegex(ValueError, "neighbour endpoint"):
            compact_caps(self.sources, self.proposal, ["native-part:240"])

    def test_insufficient_support_depth_is_refused_without_mutation(self):
        before = copy.deepcopy(self.proposal)
        with self.assertRaisesRegex(ValueError, "support-sized"):
            compact_caps(
                self.sources, self.proposal, ["native-part:239"], cap_depth_m=0.1
            )
        self.assertEqual(before, self.proposal)

    def test_narrow_source_is_refused_before_patch_preparation(self):
        sources = copy.deepcopy(self.sources)
        next(s for s in sources if s["source_id"] == "native-part:239")[
            "half_width_m"
        ] = 0.1
        with self.assertRaisesRegex(ValueError, "support disk"):
            compact_caps(sources, self.proposal, ["native-part:239"])

    def test_quarter_turn_with_translation_keeps_actual_body_and_profiles_valid(self):
        capture = copy.deepcopy(self.capture)
        capture["provenance"]["synthetic_xy_transform"] = {
            "matrix": [[0, -1], [1, 0]],
            "translation_m": [32, -64],
            "scope": "Rigid synthetic counterexample, not a new native capture.",
        }
        for way in capture["ways"]:
            for row in way["source_xy_m"] + way["sampled_design_xy_height_node"]:
                row[0], row[1] = 32 - row[1], row[0] - 64
            for span in way["recorded_profile_spans_xyz_node"]:
                for row in span:
                    row[0], row[1] = 32 - row[1], row[0] - 64
            for junction in way["prepared_junctions"]:
                for gate in junction["gates"]:
                    gate[0], gate[1] = 32 - gate[1], gate[0] - 64
        selection = [s["source_id"] for s in self.sources]
        case, sources, proposal = region_inputs(capture, selection)
        proposal = compact_caps(sources, proposal, ["native-part:239"])
        plan = prepare_profiles(
            case, sources, proposal, mesh_recipe="outward_binary_grid"
        )
        heights, note = solve_profiles(plan)
        self.assertIsNotNone(heights, note)
        body, data, _geometry = build(plan, heights)
        audit_profiles(plan, heights, data)
        vertex_links(body.vertices, body.triangles)
        products = shared_products(plan, heights, data)
        self.assertEqual(len(products["external_contacts"]), 4)

    def test_reversed_source_order_preserves_source_owned_stair_ranges(self):
        capture = copy.deepcopy(self.capture)
        capture["ways"].reverse()
        case, sources, proposal = region_inputs(
            capture, [s["source_id"] for s in self.sources]
        )
        proposal = compact_caps(sources, proposal, ["native-part:239"])
        plan = prepare_profiles(
            case, sources, proposal, mesh_recipe="outward_binary_grid"
        )
        heights, note = solve_profiles(plan)
        self.assertIsNotNone(heights, note)
        body, data, _geometry = build(plan, heights)
        audit_profiles(plan, heights, data)
        vertex_links(body.vertices, body.triangles)
        products = shared_products(plan, heights, data)
        self.assertEqual(
            {r["source_id"] for r in products["navigation"]["stair_level_ranges"]},
            {"native-part:240", "native-part:239"},
        )
        self.assertTrue(np.isfinite(data["body_vertices"]).all())


if __name__ == "__main__":
    unittest.main()
