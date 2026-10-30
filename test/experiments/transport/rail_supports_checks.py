"""Physical traffic/contact, bounded partial stations and preserved old geometry."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from elevated_rail_inputs import read_sources
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from rail_supports_context import DEFAULT_FIXTURE, SupportContext, foreign_masks
from rail_supports_plan import admit_station, footprint, plan
from stair_support_audit import Reference
from stair_support_traffic import RegionMask


def surface(z):
    return Mesh(
        np.array([[-20, -10, z], [1100, -10, z], [1100, 10, z], [-20, 10, z]], float),
        np.array([[0, 1, 2], [0, 2, 3]]),
    )


def source(key="rail", length=48, y=0):
    return {
        "source_id": key,
        "xy": np.array([[0.0, y], [length, y]]),
        "original_properties": {"subclass": "subway", "brunnel": "bridge", "layer": 1},
    }


def context(excluded=None):
    lower, upper = surface(0), surface(4)
    region = shapely.GeometryCollection() if excluded is None else excluded
    return SupportContext(
        lower,
        upper,
        SupportVolumeBuilder(lower, upper),
        Reference(lower),
        Reference(upper),
        (RegionMask(region), RegionMask(region)),
        {},
    )


class RailSupportsTests(unittest.TestCase):
    def test_rotated_lateral_footprint_has_whole_declared_dimensions(self):
        line = shapely.LineString([[0, 0], [30, 40]])
        actual = footprint(line, 20, 1.2)
        np.testing.assert_allclose(
            actual.centroid.coords[0],
            [12, 16] + np.array([-0.8, 0.6]) * 1.2,
            atol=1e-12,
        )
        self.assertAlmostEqual(actual.area, 0.72)
        for station, lateral in ((1, 0), (49, 0), (10, 1.3), (10, np.nan)):
            with self.assertRaisesRegex(ValueError, "Bounded whole"):
                footprint(line, station, lateral)

    def test_source_metadata_read_does_not_reconstruct_the_retained_deck(self):
        with patch(
            "elevated_rail_inputs.layout", side_effect=AssertionError("triangulation")
        ):
            _, _, _, rows = read_sources(DEFAULT_FIXTURE, [])
        self.assertEqual(len(rows), 3)
        self.assertTrue(
            all(r["original_properties"]["brunnel"] == "bridge" for r in rows)
        )

    def test_valid_contact_at_the_wrong_station_is_not_accepted(self):
        ctx = context()
        line = shapely.LineString([[0, 0], [48, 0]])
        product, note = ctx.builder.build(footprint(line, 18), minimum_height_m=0.1001)
        self.assertIsNotNone(product, note)
        with self.assertRaisesRegex(ValueError, "whole source station footprint"):
            admit_station(
                ctx, product, footprint(line, 6), [[], []], {"new_pier_pair_checks": 0}
            )

    def test_complete_fixed_source_has_verified_double_and_float32_contacts(self):
        products, report = plan(context(), [source()], method="fixed")
        self.assertEqual(len(products), 4)
        self.assertTrue(report["source_spacing"][0]["within_recipe_span"])
        self.assertEqual(report["source_spacing"][0]["maximum_gap_m"], 12)
        for row in report["stations"]:
            self.assertGreater(row["physical"]["float32"]["volume_m3"], 0)
            self.assertLessEqual(
                row["physical"]["lower_contact"]["float32_contact_error_m"],
                row["physical"]["lower_contact"]["derived_float32_contact_bound_m"]
                + 1e-10,
            )

    def test_old_traffic_is_rejected_before_building_and_shift_preserves_span(self):
        ctx = context(shapely.box(5, -1, 7, 1))
        products, fixed = plan(ctx, [source()], method="fixed")
        shifted, longitudinal = plan(ctx, [source()], method="longitudinal")
        self.assertEqual(len(products), 3)
        self.assertEqual(fixed["work"]["builder_calls"], 3)
        self.assertEqual(len(shifted), 4)
        self.assertEqual(longitudinal["stations"][0]["shift_m"], -2)
        self.assertLessEqual(longitudinal["source_spacing"][0]["maximum_gap_m"], 24)

    def test_precheck_avoids_a_blocked_build_without_changing_valid_body_outputs(self):
        ctx = context(shapely.box(5, -1, 7, 1))
        early, a = plan(ctx, [source()], method="fixed", precheck=True)
        late, b = plan(ctx, [source()], method="fixed", precheck=False)
        self.assertEqual(len(early), len(late))
        self.assertLess(a["work"]["builder_calls"], b["work"]["builder_calls"])
        for x, y in zip(early, late, strict=True):
            np.testing.assert_array_equal(x.body.vertices, y.body.vertices)
            np.testing.assert_array_equal(x.body.triangles, y.body.triangles)

    def test_new_piers_keep_the_foreign_distance_even_without_overlap(self):
        _, report = plan(context(), [source("a"), source("b", y=0.7)], method="fixed")
        refused = [r for r in report["stations"] if r["source_id"] == "b"]
        self.assertTrue(
            all(r["reason"] == "new_pier_foreign_distance" for r in refused)
        )
        self.assertEqual(report["accepted"], 4)

    def test_station_cap_distributes_all_sources_and_retains_every_deferred_reason(
        self,
    ):
        rows = [source(str(i), 1000) for i in range(8)]
        products, report = plan(context(), rows, method="fixed", seconds=1e-12)
        self.assertFalse(products)
        self.assertEqual(len(report["stations"]), 128)
        self.assertEqual(
            len(report["stations"]) + len(report["deferred_stations"]), 664
        )
        self.assertTrue(
            all(
                r["reason"] == "support_time_or_trial_budget"
                for r in report["stations"]
            )
        )
        self.assertTrue(
            all(
                r["reason"] == "support_station_budget"
                for r in report["deferred_stations"]
            )
        )
        for row in rows:
            nominals = [
                r["nominal_m"]
                for r in report["stations"]
                if r["source_id"] == row["source_id"]
            ]
            self.assertIn(6, nominals)
            self.assertIn(990, nominals)

    def test_ground_source_cannot_be_turned_into_an_elevated_support_recipe(self):
        row = source()
        row["original_properties"]["brunnel"] = None
        with self.assertRaisesRegex(ValueError, "elevated rail roles"):
            plan(context(), [row])

    def test_prepared_foreign_masks_match_direct_whole_traffic_distance(self):
        vertices = np.array([[0, 0, 0], [10, 0, 0], [10, 2, 0], [0, 2, 0]], float)
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        shapes = shapely.polygons(vertices[faces, :2])
        asset = SimpleNamespace(
            tree=shapely.STRtree(shapes),
            data={"body_vertices": vertices, "top_faces": faces, "body_faces": faces},
            source_ids=("old",),
            xy_rounding_m=0,
        )
        masks, work = foreign_masks(shapely.box(-2, -2, 12, 5), [asset], [])
        region = shapely.union_all(shapes)
        for y in (-0.3, -0.2, -0.1, 1, 2.1, 2.3):
            test = shapely.box(4, y, 5, y + 0.01)
            expected = bool(shapely.dwithin(test, region, 0.2))
            self.assertTrue(all(mask.conflicts(test) == expected for mask in masks))
        self.assertEqual(work["selected_assets"], 1)

    def test_rounded_foreign_neighbor_is_selected_before_the_clearance_query(self):
        vertices = np.array(
            [[1000.200025, -1, 0], [1001.0, -1, 0], [1001.0, 1, 0], [1000.200025, 1, 0]]
        )
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        rounding = float(
            np.linalg.norm(
                vertices[:, :2].astype("f4").astype(float) - vertices[:, :2], axis=1
            ).max()
        )
        asset = SimpleNamespace(
            tree=shapely.STRtree(shapely.polygons(vertices[faces, :2])),
            data={"body_vertices": vertices, "top_faces": faces, "body_faces": faces},
            source_ids=("old",),
            xy_rounding_m=rounding,
        )
        requested = shapely.box(999, -1, 1000.00002, 1)
        masks, work = foreign_masks(requested, [asset], [])
        self.assertEqual(work["selected_assets"], 1)
        self.assertFalse(masks[0].conflicts(requested))
        self.assertTrue(masks[1].conflicts(requested))


if __name__ == "__main__":
    unittest.main()
