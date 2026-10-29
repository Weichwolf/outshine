"""Bind generated endpoint seeds to actual immutable old caps and original sources."""

import unittest
from unittest.mock import patch

from stair_frontier_roots import root_seeds
from stair_frontier_sources import module_recipe
from stair_root_contact_checks import cached_ground, original_contact


def case():
    recipe, sources, *_ = original_contact()
    sources["old"]["mvt_xy"].insert(1, [2, 0])
    sources["old"]["xy_m"].insert(1, [2, 0])
    sources["branch"] = {"mvt_xy": [[2, 0], [2, 5]], "xy_m": [[2, 0], [2, 5]]}
    state = cached_ground(sources)
    records = [
        {"source_id": key, "nodes": sources[key]["mvt_xy"], "eligible": True}
        for key in ("new", "branch")
    ]
    return state, sources, records, recipe


class ActualRootDiscovery(unittest.TestCase):
    def test_legacy_interiors_keep_their_identity_and_precede_new_end_contacts(self):
        state, sources, records, _ = case()
        roots, deferred = root_seeds(state, sources, records)
        self.assertFalse(deferred)
        self.assertEqual(
            roots[0],
            {
                "root_id": "old@1",
                "node": [2, 0],
                "port": {"source_id": "old", "vertex": 1},
            },
        )
        self.assertEqual(
            roots[1],
            {
                "root_id": "old@0/new",
                "node": [0, 0],
                "port": {"source_id": "old", "end": 0},
                "endpoint": True,
                "initial_source_id": "new",
            },
        )
        self.assertEqual(root_seeds(state, sources, records[::-1]), (roots, deferred))

    def test_existing_profile_probe_budget_is_shared_without_displacing_old_roots(self):
        state, sources, records, _ = case()
        with patch("stair_frontier_roots.root_profile") as probe:
            roots, deferred = root_seeds(state, sources, records, max_profiles=1)
        self.assertEqual(probe.call_count, 1)
        self.assertEqual([r["root_id"] for r in roots], ["old@1"])
        self.assertEqual(deferred[0]["reason"], "verified_seed_probe_budget")
        with patch("stair_frontier_roots.root_profile") as probe:
            roots, deferred = root_seeds(state, sources, records, max_profiles=0)
        probe.assert_not_called()
        self.assertFalse(roots)
        self.assertEqual(len(deferred), 2)

    def test_wrong_frame_ground_and_profile_axis_cannot_supply_endpoint_roots(self):
        for wrong in ("frame", "ground", "axis"):
            state, sources, records, _ = case()
            if wrong == "frame":
                sources["new"]["xy_m"][1][0] += 1e-12
            elif wrong == "ground":
                state["road_ground"][:] = False
            else:
                state["cached_module_rows"]["old"]["xy"] = state["cached_module_rows"][
                    "old"
                ]["xy"].astype(float)
                state["cached_module_rows"]["old"]["xy"][0, 0] += 0.001
            roots, deferred = root_seeds(state, sources, records)
            with self.subTest(wrong=wrong):
                self.assertFalse(any(r.get("endpoint") for r in roots))
                self.assertTrue(any(d["root_id"] == "old@0/new" for d in deferred))

    def test_repeated_original_new_occurrences_are_not_seed_contacts(self):
        state, sources, records, _ = case()
        sources["new"]["mvt_xy"].append([0, 0])
        sources["new"]["xy_m"].append([0, 0])
        roots, _ = root_seeds(state, sources, records)
        self.assertFalse(any(r.get("endpoint") for r in roots))

    def test_endpoint_only_sources_do_not_claim_a_new_through_role(self):
        state, sources, records, _ = case()
        sources["new"]["mvt_xy"][:] = sources["new"]["mvt_xy"][1:]
        sources["new"]["xy_m"][:] = sources["new"]["xy_m"][1:]
        roots, _ = root_seeds(state, sources, records)
        self.assertFalse(any(r.get("endpoint") for r in roots))

    def test_recipe_prefix_cannot_drop_its_bound_first_whole_source(self):
        state, sources, records, _ = case()
        root = root_seeds(state, sources, records)[0][-1]
        with self.assertRaisesRegex(ValueError, "explicit first source"):
            module_recipe({}, state, sources, root, {"source_ids": ["branch"]})


if __name__ == "__main__":
    unittest.main()
