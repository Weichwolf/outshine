"""Raised decks preserve underpasses while actual piers and rail headroom block."""

import unittest
from dataclasses import replace

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval, LocalFrame, Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from rail_building_grid import BAND_M, construct
from rail_occupancy_guard import RoundingGuardQuery, round_bounds
from rail_occupancy_inputs import RailProducts, deck_asset, supplements
from rail_occupancy_reference import whole_raised
from shared_bridge_geometry import layout
from stair_occupancy_candidate_sources import bound_support_sources
from stair_occupancy_candidates import CandidateQuery, CandidateVolumes, VolumeSource
from stair_occupancy_geometry import OccupancyError
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

    def test_real_rounded_cut_crossing_requires_construction_room(self):
        a = np.deg2rad(33)
        matrix = np.array([[np.cos(a), -np.sin(a)], [np.sin(a), np.cos(a)]])

        def rotate(mesh):
            return Mesh(
                np.c_[mesh.vertices[:, :2] @ matrix.T, mesh.vertices[:, 2]],
                mesh.triangles,
            )

        rail = replace(
            self.rail,
            rows=[{**r, "xy": r["xy"] @ matrix.T} for r in self.rail.rows],
            deck=rotate(self.rail.deck),
            supports=[rotate(b) for b in self.rail.supports],
            contacts=[(rotate(l), rotate(h)) for l, h in self.rail.contacts],
        )
        query = CandidateQuery(CandidateVolumes(self.frame, supplements(rail)))
        probe = BuildingFootprint(
            "building", shapely.box(5, 0, 10, 8), HeightInterval(6, 7)
        )
        rounded = OccupancyQuery(whole_raised(rail, cast=True)[0])

        def actual_conflict(cut):
            shape = shapely.transform(
                probe.geometry.difference(cut),
                lambda xy: xy.astype("f4").astype(float),
            )
            return rounded.conflict(BuildingFootprint("actual", shape, probe.heights))[
                0
            ]

        raw, _work = query.conflict(probe)
        self.assertGreater(actual_conflict(raw).area, 0)
        guarded = RoundingGuardQuery(query, round_bounds(rail))
        cut, work = guarded.conflict(probe)
        self.assertTrue(actual_conflict(cut).is_empty)
        self.assertTrue(raw.difference(cut).is_empty)
        self.assertLess(work["derived_xy_guard_m"], 0.005)

    def test_old_and_raised_queries_share_one_product_capacity(self):
        far = np.array([[100, 100, 0, 101, 101, 1]], float)
        far.setflags(write=False)

        def forbidden(_face):
            raise AssertionError("Far metadata cannot construct geometry")

        old = CandidateVolumes(
            self.frame,
            tuple(VolumeSource(f"old:{i}", far, forbidden) for i in range(255)),
        )
        raised = CandidateVolumes(self.frame, supplements(self.rail))
        query = RoundingGuardQuery(
            CandidateQuery(
                CandidateVolumes(self.frame, (*old.sources, *raised.sources))
            ),
            round_bounds(self.rail),
        )
        probe = BuildingFootprint(
            "underpass", shapely.box(1, 0, 2, 1), HeightInterval(0, 1)
        )
        with self.assertRaisesRegex(OccupancyError, "product_budget_exhausted"):
            query.conflict(probe)


class BinaryBuildingConstruction(unittest.TestCase):
    def test_binary_domain_keeps_courtyard_and_actual_rounded_roof(self):
        from building_mesh import solid
        from stair_consumer_buildings import projected_domain

        original = shapely.Polygon(
            [
                (0.1234567, 0),
                (10.1234567, 0),
                (10.1234567, 4),
                (5.1234567, 4),
                (5.1234567 + 1e-12, 4 - 1e-12),
                (0.1234567, 4),
            ],
            holes=[[(2, 1), (3, 1), (3, 2), (2, 2)]],
        )
        actual, note = construct(original)
        self.assertEqual(note["requested_holes"], note["constructed_holes"])
        self.assertTrue(original.buffer(-BAND_M, join_style="mitre").covered_by(actual))
        self.assertTrue(actual.covered_by(original.buffer(BAND_M, join_style="mitre")))
        storage = ([], [], [], [])
        solid(storage, actual, 0, 4, 0)
        body = Mesh(storage[0], storage[1])
        np.testing.assert_array_equal(
            body.vertices, body.vertices.astype("f4").astype(float)
        )
        self.assertTrue(projected_domain(body).equals(actual))
        self.assertEqual(note["omitted_requested_parts"], [])

    def test_whole_collapse_and_courtyard_filling_are_refused(self):
        with self.assertRaisesRegex(ValueError, "collapses"):
            construct(shapely.box(0, 0, 1e-7, 1e-7))
        source = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (0, 4)],
            holes=[[(2, 2), (2 + 1e-7, 2), (2 + 1e-7, 2 + 1e-7), (2, 2 + 1e-7)]],
        )
        with self.assertRaisesRegex(ValueError, "courtyard"):
            construct(source)

    def test_local_collapsed_component_retains_original_shape_and_reason(self):
        source = shapely.MultiPolygon(
            [shapely.box(0, 0, 4, 4), shapely.box(10, 10, 10 + 1e-7, 10 + 1e-7)]
        )
        actual, note = construct(source)
        self.assertTrue(actual.equals(shapely.box(0, 0, 4, 4)))
        self.assertEqual(note["requested_parts"], 2)
        self.assertEqual(note["constructed_parts"], 1)
        self.assertEqual(
            note["omitted_requested_parts"],
            [
                {
                    "input_part": 1,
                    "reason": "below_binary_construction_precision",
                    "footprint_wkb": shapely.get_parts(source)[1].wkb_hex,
                }
            ],
        )

    def test_far_coordinates_and_invalid_domains_are_not_silently_snapped(self):
        for source in (
            shapely.box(8190, 0, 8191, 1),
            shapely.Polygon([(0, 0), (2, 2), (0, 2), (2, 0)]),
        ):
            with self.assertRaisesRegex(ValueError, "valid local"):
                construct(source)


if __name__ == "__main__":
    unittest.main()
