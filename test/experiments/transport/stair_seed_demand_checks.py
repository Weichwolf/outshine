"""Whole requested sources, conservative early rejection and real old-ground guards."""

import copy
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from stair_endpoint_frontier_study import QueryContext, select
from stair_frontier_sources import records_for
from stair_module_inputs import reference_profiles
from stair_seed_sources import separated_records


def source(xy, **properties):
    return {
        "properties": {"class": "path", "subclass": "footway", **properties},
        "xy_m": xy,
        "mvt_xy": [[i, 0] for i in range(len(xy))],
        "shape": shapely.LineString(xy),
    }


def state_for():
    return {
        "plan": {"xy": np.array([[-100, -100], [100, 100]], float)},
        "report": {"candidate_source_ids": []},
        "cached_module_rows": {},
    }


def view_for():
    return SimpleNamespace(
        patches=[SimpleNamespace(bounds_xy_m=(-100, -100, 100, 100))]
    )


class CountedTree:
    def __init__(self, shape):
        self.actual = shapely.STRtree([shape])
        self.queries = 0

    def query(self, *args, **kwargs):
        self.queries += 1
        return self.actual.query(*args, **kwargs)


def asset_at(shape):
    return SimpleNamespace(
        tree=CountedTree(shape), data={"ground": np.ones(1, bool)}, xy_rounding_m=0
    )


class RequestedSeedSources(unittest.TestCase):
    def test_whole_long_source_is_kept_when_only_its_middle_is_requested(self):
        sources = {"long": source([[-50, 0], [50, 0]])}
        before = copy.deepcopy(sources)
        rows = records_for(state_for(), sources, requested_bounds=[-1, -1, 1, 1])
        self.assertTrue(rows[0]["eligible"])
        self.assertEqual(rows[0]["nodes"], before["long"]["mvt_xy"])
        self.assertEqual(rows[0]["length_m"], 100)
        self.assertEqual(sources, before)

    def test_corridor_touching_request_is_kept_even_when_axis_is_outside(self):
        sources = {"edge": source([[-10, 0], [10, 0]])}
        half = reference_profiles()["footway"]["widthM"] / 2
        row = records_for(
            state_for(), sources, requested_bounds=[-1, half - 0.05, 1, half + 0.05]
        )[0]
        self.assertTrue(row["eligible"])

    def test_mitre_extremes_survive_the_conservative_numeric_prefilter(self):
        for angle in (5, 15, 45, 90, 120, 150, 175, 179.99999):
            radians = np.deg2rad(angle)
            xy = [[0, 0], [10, 0], [10 + 10 * np.cos(radians), 10 * np.sin(radians)]]
            row = source(xy)
            band = row["shape"].buffer(
                reference_profiles()["footway"]["widthM"] / 2,
                cap_style="flat",
                join_style="mitre",
                mitre_limit=2,
            )
            for point in shapely.get_coordinates(band):
                bounds = np.r_[point - 1e-5, point + 1e-5]
                with self.subTest(angle=angle, point=point.tolist()):
                    self.assertTrue(
                        records_for(state_for(), {"a": row}, requested_bounds=bounds)[
                            0
                        ]["eligible"]
                    )

    def test_outside_demand_does_not_build_corridors_or_query_old_bodies(self):
        sources = {"near": source([[-4, 0], [4, 0]])}
        sources.update(
            {f"far-{i}": source([[70, 60 + i], [80, 60 + i]]) for i in range(10)}
        )
        old = asset_at(shapely.box(95, 95, 96, 96))
        rows, work = separated_records(
            view_for(),
            [old],
            {"source_ids": []},
            sources,
            requested_bounds=[-5, -1, 5, 1],
        )
        self.assertEqual([r["source_id"] for r in rows if r["eligible"]], ["near"])
        self.assertEqual(old.tree.queries, 1)
        self.assertEqual(work["source_selection"]["corridor_builds"], 1)
        self.assertEqual(work["old_probe_corridor_builds"], 1)
        self.assertEqual(work["source_selection"]["request_bounds_rejections"], 10)
        self.assertTrue(
            all(
                r["reason"] == "outside_requested_source_bounds"
                for r in rows
                if r["source_id"] != "near"
            )
        )

    def test_old_ground_outside_request_still_blocks_the_complete_new_source(self):
        sources = {"long": source([[-50, 0], [50, 0]])}
        old = asset_at(shapely.box(34, -1, 36, 1))
        rows, work = separated_records(
            view_for(),
            [old],
            {"source_ids": []},
            sources,
            requested_bounds=[-1, -1, 1, 1],
        )
        self.assertFalse(rows[0]["eligible"])
        self.assertEqual(rows[0]["reason"], "seed_requires_explicit_old_ground_contact")
        self.assertGreater(work["candidate_old_facets"], 0)

    def test_cached_and_wrong_level_sources_keep_their_original_semantics(self):
        sources = {
            "cached": source([[70, 60], [80, 60]]),
            "indoor": source([[0, 0], [5, 0]], indoor=1),
            "bridge": source([[0, 0], [5, 0]], brunnel="bridge"),
        }
        state = state_for()
        state["resident_source_ids"] = ["cached"]
        rows = records_for(state, sources, requested_bounds=[-1, -1, 1, 1])
        self.assertEqual(
            {r["source_id"]: r["reason"] for r in rows},
            {
                "cached": "already_cached",
                "indoor": "indoor_recipe_missing",
                "bridge": "vertical_structure_recipe_missing",
            },
        )

    def test_invalid_or_unbounded_request_is_never_empty_free_space(self):
        sources = {"a": source([[0, 0], [10, 0]])}
        for bounds in (
            [0, 0, 0, 1],
            [2, 0, 1, 1],
            [0, 0, np.nan, 1],
            [0, 0, 1e7, 1],
            [0, 1],
        ):
            with self.subTest(bounds=bounds), self.assertRaises(ValueError):
                records_for(state_for(), sources, requested_bounds=bounds)

    def test_no_cached_profile_uses_real_seed_recipes_without_a_fake_port(self):
        state = state_for()
        state["plan"]["xy"] = np.array([[-2, -2], [2, 2]], float)
        manifest = {
            "source_ids": [],
            "root_resident_sha256": "test",
            "parent_reports": [],
        }
        context = QueryContext(
            state,
            {"long": source([[-50, 0], [50, 0]])},
            {},
            {},
            manifest,
            view_for(),
            (),
        )
        with patch("stair_endpoint_frontier_study.query_context", return_value=context):
            result = select(SimpleNamespace())
        self.assertEqual(result["family"], "seed")
        self.assertEqual(result["recipes"][0]["contact_mode"], "seed")
        self.assertEqual(result["recipes"][0]["source_ids"], ["long"])
        self.assertNotIn("port", result["recipes"][0])

    def test_valid_attached_recipe_performs_no_seed_work(self):
        context = QueryContext(
            {},
            {},
            {},
            {"held": True},
            {"source_ids": [], "root_resident_sha256": "test", "parent_reports": []},
            object(),
            (),
        )
        recipe = {"contact_mode": "profile", "source_ids": ["a"]}
        with (
            patch("stair_endpoint_frontier_study.query_context", return_value=context),
            patch(
                "stair_endpoint_frontier_study.proposals",
                return_value=({}, [], [], [], [recipe]),
            ),
            patch(
                "stair_endpoint_frontier_study.select_seed",
                side_effect=AssertionError("unnecessary seed work"),
            ),
        ):
            result = select(SimpleNamespace())
        self.assertEqual(result["recipes"], [recipe])
        self.assertNotIn("family", result)
        self.assertNotIn("seed_work", result)


if __name__ == "__main__":
    unittest.main()
