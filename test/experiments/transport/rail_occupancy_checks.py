"""Raised decks preserve underpasses while actual piers and rail headroom block."""

import unittest
from dataclasses import replace

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval, LocalFrame, Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from rail_occupancy_inputs import RailProducts, deck_asset, supplements
from rail_occupancy_reference import whole_raised
from shared_bridge_geometry import layout
from stair_occupancy_candidate_sources import bound_support_sources
from stair_occupancy_candidates import CandidateQuery, CandidateVolumes
from stair_occupancy_handoff import OccupancyQuery
from surface_solid_methods import close_surface


def products():
    rows = [
        {
            "source_id": "original-raised-rail",
            "xy": np.array([[0.1234567, 0], [20.1234567, 0]]),
            "width_m": 3.8,
            "end_heights_m": [6.1234567, 6.1234567],
        }
    ]
    top, _domain, _note = layout(rows)
    body = close_surface(top, 0.6)
    upper = Mesh(body.vertices[len(top.vertices) :], top.triangles)
    lower = Mesh(
        [[-1, -3, 0], [22, -3, 0], [22, 3, 0], [-1, 3, 0]],
        [[0, 1, 2], [0, 2, 3]],
    )
    support, _note = SupportVolumeBuilder(lower, upper).build(
        shapely.box(7.4, -0.3, 8.6, 0.3), minimum_height_m=0.1
    )
    if support is None:
        raise AssertionError("Fixture requires an actual whole support")
    return RailProducts(
        (),
        {"original-raised-rail": {"properties": {"subclass": "subway"}}},
        rows,
        body,
        len(top.vertices),
        top.triangles,
        [support.body],
        [(support.lower_contact, support.upper_contact)],
        {0: "original-raised-rail"},
        "held-deck",
        "held-support",
    )


class RaisedVolumeHandoff(unittest.TestCase):
    def setUp(self):
        self.rail = products()
        self.frame = LocalFrame("raised-occupancy-fixture")

    def test_quantization_retains_original_facet_owners_and_elevated_role(self):
        a, b = deck_asset(self.rail), deck_asset(self.rail, cast=True)
        self.assertEqual(a.source_ids, b.source_ids)
        self.assertFalse(a.data["ground"].any())
        self.assertFalse(b.data["ground"].any())
        np.testing.assert_array_equal(a.data["owners"], b.data["owners"])
        self.assertTrue(np.any(a.data["body_vertices"] != b.data["body_vertices"]))

    def test_actual_underdeck_pier_and_train_headroom_in_both_precisions(self):
        cases = (
            ("low-free-under-deck", (2, 0), (0, 1.2), False),
            ("actual-pier", (8, 0), (0, 1.2), True),
            ("actual-deck", (2, 0), (5.6, 6.2), True),
            ("train-headroom", (2, 0), (7, 8), True),
            ("above-headroom", (2, 0), (12, 13), False),
            ("beside-pier-under-deck", (8, 1), (0, 1.2), False),
        )
        for cast in (False, True):
            sources = supplements(self.rail, cast=cast)
            query = CandidateQuery(CandidateVolumes(self.frame, sources))
            whole, _owners = whole_raised(self.rail, cast=cast)
            reference = OccupancyQuery(whole)
            for name, (x, y), heights, blocked in cases:
                with self.subTest(cast=cast, name=name):
                    probe = BuildingFootprint(
                        name,
                        shapely.box(x - 0.1, y - 0.1, x + 0.1, y + 0.1),
                        HeightInterval(*heights),
                    )
                    cut, _work = query.conflict(probe)
                    all_cut, _work = reference.conflict(probe, method="scan")
                    self.assertEqual(not cut.is_empty, blocked)
                    self.assertTrue(cut.equals(all_cut))

    def test_free_underdeck_rejects_metadata_before_affine_materialization(self):
        query = CandidateQuery(CandidateVolumes(self.frame, supplements(self.rail)))
        probe = BuildingFootprint(
            "underpass", shapely.box(1, -0.1, 2, 0.1), HeightInterval(0, 1.2)
        )
        cut, cost = query.conflict(probe)
        self.assertTrue(cut.is_empty)
        self.assertEqual(cost["metadata_materializer_calls"], 0)
        self.assertEqual(cost["metadata_face_tests"], 0)

    def test_source_domain_is_bound_before_quantization(self):
        moved = [{**self.rail.rows[0], "xy": self.rail.rows[0]["xy"] + [100, 0]}]
        for cast in (False, True):
            with self.assertRaisesRegex(ValueError, "source-domain ownership"):
                deck_asset(replace(self.rail, rows=moved), cast=cast)

    def test_actual_support_cannot_be_replaced_by_its_caps_only(self):
        body = self.rail.supports[0]
        changed = Mesh(body.vertices + [0, 0, 0.01], body.triangles)
        with self.assertRaisesRegex(ValueError, "original support caps"):
            bound_support_sources(
                [changed], self.rail.contacts, self.rail.owners, "rebound-identity"
            )

    def test_each_saved_support_retains_one_original_source_owner(self):
        for owners in ({}, {0: ""}, {0: "rail", 1: "extra"}):
            with self.assertRaisesRegex(ValueError, "support ownership"):
                bound_support_sources(
                    self.rail.supports, self.rail.contacts, owners, "identity"
                )


if __name__ == "__main__":
    unittest.main()
