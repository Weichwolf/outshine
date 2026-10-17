"""Stable existing SITE inputs, bounded refill, and independent identity checks."""

import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import (
    BuildingFootprint,
    HeightInterval,
    LocalFrame,
    Mesh,
    Reservation,
)
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from recipe_graph_data import plain
from recipe_plant_operators import SITE, prepared
from recipe_plant_values import seed_input
from stair_occupancy_handoff import OccupancyQuery
from stair_plant_retained_verify import verify_retention
from stair_plant_retention import compatible_source
from stair_plant_sites import candidate, plants, region
from stair_plant_verify import FullScan


def world(half=16, height=10, frame="world"):
    vertices = np.array(
        [
            [-half, -half, height],
            [half, -half, height],
            [half, half, height],
            [-half, half, height],
        ],
        float,
    )
    mesh = Mesh(vertices, np.array([[0, 1, 2], [0, 2, 3]], np.uint32))
    frame = LocalFrame(frame)
    return {
        "terrain": mesh,
        "sampler": SurfaceSampler(mesh),
        "frame": frame,
        "terrain_patches": SurfacePatches(SurfacePatch("root", frame, "soil", mesh)),
    }


def inputs_for_sites():
    data = world()
    query = OccupancyQuery(
        (
            Reservation(
                "far",
                shapely.box(100, 100, 101, 101),
                HeightInterval(0, 100),
                "control",
            ),
        )
    )
    recipe = plain(
        prepared()
        .evaluate({"seed": seed_input(718)}, outputs=("plant",))
        .outputs["plant"]
        .value
    )
    proto = {"recipe": recipe, "bounds_m": [[-2, -2, 0], [2, 2, 4]]}
    rows, records = [], []
    lower, side, key = region(data)
    graph = prepared(SITE)
    for i, x in enumerate((-6.0, -2.0, 2.0, 6.0)):
        geometry = shapely.box(x - 1.5, -1.5, x + 1.5, 1.5)
        row = {
            "id": f"source:{i}:part:0",
            "source_id": f"source:{i}",
            "part": 0,
            "polygon": geometry,
            "properties": {"class": "wood"},
            "original_feature": {"id": i, "properties": {"class": "wood"}},
            "projected_original_wkb": geometry.wkb_hex,
        }
        value = candidate(
            row,
            np.array([x, 0]),
            (0, 0, 0, 0, 0),
            geometry,
            data,
            query,
            proto,
            (lower, side, key + ":" + row["id"]),
            graph,
        )
        rows.append(row)
        records.append(
            {
                "source_part_id": row["id"],
                "source_id": row["source_id"],
                "part": 0,
                "result": value,
            }
        )
    prior = {
        "report": {"frame": "world"},
        "compatible": True,
        "records": records,
        "sources": {
            r["id"]: {k: v for k, v in r.items() if k != "polygon"} for r in rows
        },
        "proof": {"products": "prior", "receipt": "proof"},
    }
    return data, query, proto, rows, prior


class RetainedSites(unittest.TestCase):
    def run_sites(self, data, query, proto, rows, prior, **limits):
        with patch("stair_plant_sites.source_rows", return_value=(rows, [])):
            return plants(data, query, proto, prior=prior, **limits)

    def test_four_valid_sites_need_no_world_union_or_new_proposals(self):
        data, query, proto, rows, prior = inputs_for_sites()
        with (
            patch(
                "stair_plant_sites.terrain_domain",
                side_effect=AssertionError("world union"),
            ),
            patch(
                "stair_plant_sites.proposals",
                side_effect=AssertionError("new proposals"),
            ),
        ):
            accepted, ledger, budget = self.run_sites(data, query, proto, rows, prior)
        self.assertEqual(len(accepted), 4)
        self.assertEqual(budget["attempts"], 4)
        self.assertEqual(budget["proposals"], 0)
        self.assertEqual(budget["fresh_attempts"], 0)
        self.assertEqual(len(ledger), 4)
        for old, new in zip(prior["records"], accepted, strict=True):
            for field in ("site_id", "world_origin_m", "placement", "habitat"):
                self.assertEqual(old["result"][field], new["result"][field])

    def test_world_growth_preserves_old_region_and_site_coordinates(self):
        _data, query, proto, rows, prior = inputs_for_sites()
        grown = world(32)
        self.assertNotEqual(
            region(grown)[0], tuple(prior["records"][0]["result"]["habitat"]["lower"])
        )
        accepted, _, budget = self.run_sites(grown, query, proto, rows, prior)
        self.assertEqual(budget["retained_plants"], 4)
        for old, new in zip(prior["records"], accepted, strict=True):
            self.assertEqual(
                old["result"]["world_origin_m"], new["result"]["world_origin_m"]
            )
            self.assertEqual(
                old["result"]["habitat"]["lower"], new["result"]["habitat"]["lower"]
            )

    def test_new_profile_blocks_overhang_without_discarding_other_old_sites(self):
        data, _query, proto, rows, prior = inputs_for_sites()
        shape = shapely.box(-4.6, -0.1, -4.1, 0.1)
        old = OccupancyQuery(
            (Reservation("old-profile", shape, HeightInterval(6, 8.5), "traffic"),)
        )
        current = OccupancyQuery(
            (Reservation("new-profile", shape, HeightInterval(6, 10.5), "traffic"),)
        )
        body = BuildingFootprint(
            "whole-crown", shapely.box(-8, -2, -4, 2), HeightInterval(10, 14)
        )
        self.assertTrue(old.conflict(body)[0].is_empty)
        accepted, _, budget = self.run_sites(
            data, current, proto, rows, prior, max_attempts=4
        )
        self.assertEqual(len(accepted), 3)
        self.assertEqual(budget["retention"][0]["state"], "deferred")
        self.assertIn("whole measured plant envelope", budget["retention"][0]["reason"])
        self.assertEqual(budget["fresh_attempts"], 0)

    def test_changed_terrain_is_not_silently_used_as_the_old_position(self):
        _data, query, proto, rows, prior = inputs_for_sites()
        accepted, _, budget = self.run_sites(
            world(height=11), query, proto, rows, prior, max_attempts=4
        )
        self.assertEqual(accepted, [])
        self.assertTrue(
            all(
                n["reason"] == "retained SITE root position changed"
                for n in budget["retention"]
            )
        )

    def test_source_frame_definition_and_missing_source_invalidate_prior_identity(self):
        data, _query, _proto, rows, prior = inputs_for_sites()
        record = prior["records"][0]
        changed = copy.deepcopy(rows[0])
        changed["properties"]["class"] = "grass"
        self.assertEqual(
            compatible_source(prior, record, changed, "world"),
            "original_source_part_changed",
        )
        self.assertEqual(
            compatible_source(prior, record, None, "world"),
            "original_source_part_missing",
        )
        self.assertEqual(
            compatible_source(prior, record, rows[0], "other"), "world_frame_changed"
        )
        prior["compatible"] = False
        self.assertEqual(
            compatible_source(prior, record, rows[0], data["frame"].identifier),
            "plant_definition_or_seed_changed",
        )

    def test_zero_demand_has_no_proposals_or_site_graph_work(self):
        data, query, proto, rows, prior = inputs_for_sites()
        with (
            patch(
                "stair_plant_sites.proposals", side_effect=AssertionError("proposal")
            ),
            patch(
                "recipe_plant_operators.prepared", side_effect=AssertionError("graph")
            ),
        ):
            accepted, _, budget = self.run_sites(
                data, query, proto, rows, prior, max_plants=0
            )
        self.assertEqual(
            (accepted, budget["attempts"], budget["proposals"]), ([], 0, 0)
        )

    def test_a_different_old_pose_contract_is_not_reset_to_identity(self):
        data, query, proto, rows, prior = inputs_for_sites()
        prior["records"][0]["result"]["placement"]["frame"] = [
            [0.0, -1.0, 0.0],
            [1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0],
        ]
        accepted, _, budget = self.run_sites(
            data, query, proto, rows, prior, max_attempts=4
        )
        self.assertEqual(len(accepted), 3)
        self.assertEqual(
            budget["retention"][0]["reason"], "retained SITE placement contract changed"
        )

    def test_only_missing_capacity_is_refilled_with_unique_source_site_keys(self):
        data, query, proto, rows, prior = inputs_for_sites()
        rows = copy.deepcopy(rows)
        rows[0]["properties"]["class"] = "water"
        rows[0]["original_feature"]["properties"]["class"] = "water"
        accepted, _, budget = self.run_sites(data, query, proto, rows, prior)
        self.assertEqual(budget["retained_plants"], 3)
        self.assertEqual(len(accepted), 4)
        self.assertEqual(
            len(
                {(p["source_part_id"], tuple(p["result"]["site_id"])) for p in accepted}
            ),
            4,
        )
        self.assertGreater(budget["fresh_attempts"], 0)
        self.assertEqual(
            budget["retention"][0]["reason"], "original_source_part_changed"
        )

    def test_cached_contact_must_belong_to_the_requested_root_xy(self):
        data, query, proto, rows, _prior = inputs_for_sites()
        row = rows[0]
        wrong = data["sampler"].sample([[-5.9, 0]])
        lower, side, key = region(data)
        with self.assertRaisesRegex(ValueError, "another query point"):
            candidate(
                row,
                np.array([-6.0, 0]),
                (0, 0, 0, 0, 0),
                row["polygon"],
                data,
                query,
                proto,
                (lower, side, key + ":" + row["id"]),
                prepared(SITE),
                contact=wrong,
            )

    def test_independent_identity_audit_rejects_a_missing_or_silently_deferred_old_site(
        self,
    ):
        data, query, proto, rows, prior = inputs_for_sites()
        accepted, _, budget = self.run_sites(data, query, proto, rows, prior)
        report = {
            "prior_plants": prior["proof"],
            "budget": budget,
            "prototype_preparation": {"reused": True},
        }
        graph = prepared(SITE)
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-sites-audit-"
        ) as root:
            output = Path(root)
            path = output / "plants.json"
            path.write_text(json.dumps(accepted))
            with patch("stair_plant_retained_verify.previous", return_value=prior):
                verify_retention(
                    output, data, FullScan(query), proto, report, rows, graph, candidate
                )
                path.write_text(json.dumps(accepted[:1] + accepted[2:]))
                with self.assertRaises(ValueError):
                    verify_retention(
                        output,
                        data,
                        FullScan(query),
                        proto,
                        report,
                        rows,
                        graph,
                        candidate,
                    )
                path.write_text(json.dumps(accepted))
                bad = copy.deepcopy(report)
                bad["budget"]["retention"][1].update(
                    state="deferred", reason="invented obstruction"
                )
                with self.assertRaisesRegex(ValueError, "silently discarded"):
                    verify_retention(
                        output,
                        data,
                        FullScan(query),
                        proto,
                        bad,
                        rows,
                        graph,
                        candidate,
                    )


if __name__ == "__main__":
    unittest.main()
