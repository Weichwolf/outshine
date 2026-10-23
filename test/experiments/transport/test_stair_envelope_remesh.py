import copy
import json
import unittest
from unittest.mock import patch

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_solve import solve_profiles
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_envelope_remesh import coarsen_envelope
from stair_indexed_levels import LevelCuts
from stair_junction_body import build


def counted_build(plan, heights):
    calls = [0]
    original = LevelCuts.crossing

    def counted(self, *args):
        calls[0] += 1
        return original(self, *args)

    with patch.object(LevelCuts, "crossing", counted):
        body, data, note = build(plan, heights)
    return body, data, note, calls[0]


class EnvelopeBeforeTreadsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        case, sources = inputs()
        native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        cls.plan, cls.contacts = prepare_native_body_plan(
            case, sources, plan_connections(sources), native
        )
        cls.heights, solve = solve_profiles(
            cls.plan,
            fixed_contacts=cls.contacts,
            max_prior_change_m=3,
            gradient_model="axis_crossfall",
        )
        if cls.heights is None:
            raise ValueError(solve)
        before = copy.deepcopy(cls.plan)
        before["tread_partition_recipe"] = "indexed_band_remesh"
        cls.before = counted_build(before, cls.heights)
        cls.after = counted_build(cls.plan, cls.heights)

    def test_same_body_quality_needs_fewer_real_edge_queries_and_intersections(self):
        old_body, _old_data, old, old_calls = self.before
        body, _data, new, calls = self.after
        self.assertEqual(old["edge_queries"], old_calls)
        self.assertEqual(new["edge_queries"], calls)
        self.assertLess(calls, old_calls / 3)
        self.assertLess(new["edge_intersections"], old["edge_intersections"] / 3)
        self.assertLess(
            new["band_remesh"]["input_faces"], old["band_remesh"]["input_faces"] / 2
        )
        self.assertEqual(len(body.vertices), len(old_body.vertices))
        self.assertEqual(len(body.triangles), len(old_body.triangles))
        for _body, data, _note, _calls in (self.before, self.after):
            plan, heights = consumed_plan(self.plan, data)
            proof = audit_profiles(
                plan,
                heights,
                data,
                max_prior_change_m=3,
                gradient_model="axis_crossfall",
            )
            self.assertEqual(proof["body"]["treads"]["risers"], 18)
            self.assertLess(
                max(
                    audit_rendered_contacts(plan, data, self.contacts)[
                        "height_errors_m"
                    ].values()
                ),
                1e-10,
            )

    def test_mutual_reference_samples_preserve_each_sources_height_field(self):
        plans = [
            consumed_plan(self.plan, data)
            for _body, data, _note, _calls in (self.before, self.after)
        ]
        for a, b in (plans, plans[::-1]):
            old, old_z = a
            new, new_z = b
            for owner in range(len(old["rows"])):
                vertices = np.unique(old["parts"][owner])
                sampler = SurfaceSampler(
                    Mesh(np.c_[new["xy"], new_z], new["parts"][owner]),
                    xy_tolerance_m=1e-12,
                )
                samples = sampler.sample(old["xy"][vertices])
                self.assertFalse(any(samples.reasons))
                self.assertLess(
                    abs(samples.points_enu_m[:, 2] - old_z[vertices]).max(), 1e-10
                )

    def test_preparation_keeps_original_plan_vertices_and_other_source_faces(self):
        original = copy.deepcopy(self.plan)
        coarse, work = coarsen_envelope(self.plan, self.heights)
        self.assertIs(coarse["xy"], self.plan["xy"])
        self.assertEqual(work["groups"], 3)
        self.assertLess(work["output_faces"], work["input_faces"])
        for owner in (1, 2):
            self.assertEqual(
                sorted(map(tuple, coarse["parts"][owner])),
                sorted(map(tuple, self.plan["parts"][owner])),
            )
        for a, b in zip(original["parts"], self.plan["parts"], strict=True):
            np.testing.assert_array_equal(a, b)

    def test_nonplanar_flight_is_rejected_before_new_tread_generation(self):
        heights = self.heights.copy()
        flight = self.plan["stair_lofts"][0]
        vertices = np.unique(self.plan["parts"][flight["owner"]])
        station = (self.plan["xy"][vertices] - flight["origin_xy_m"]) @ flight[
            "tangent"
        ]
        inside = vertices[
            (station >= flight["start_cut_m"] - 1e-8)
            & (station <= flight["end_cut_m"] + 1e-8)
        ]
        self.assertGreater(len(inside), 0)
        heights[inside[0]] += 0.01
        with self.assertRaisesRegex(ValueError, "declared flat/linear/flat field"):
            coarsen_envelope(self.plan, heights)
