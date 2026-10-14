"""Shared-source topology, complete clearance, soft ports and pinned source checks."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import close_surface
from shared_bridge_geometry import corridor, layout
from shared_bridge_products import products
from shared_bridge_recipe import build
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed

FIXTURE = Path(__file__).with_name("fixtures") / "shared-bridge-vienna.json"
PIN = "c6e99c52ded6a696c0fe4a8f31be48143abbc0a21ab331e666dffc491a162ff5"


def source(identity, xy, *, role="ground", heights=(0.0, 0.0), width=4.0):
    return {
        "source_id": identity,
        "xy": xy,
        "role": role,
        "end_heights_m": list(heights),
        "width_m": width,
        "properties": {"layer": 1, "brunnel": "bridge"}
        if role == "deck"
        else {"layer": 0},
    }


def controls():
    return [
        source("deck-first", [[0, 0], [100, 0]], role="deck"),
        source("deck-second", [[100, 0], [200, 0]], role="deck"),
        source("lower-first", [[30, -20], [30, 20]], heights=(0, 0.4)),
        source("lower-second", [[150, -20], [150, 20]], heights=(1, 1.2)),
        source("lower-join", [[30, 10], [150, 10]], heights=(0.3, 1.1)),
    ]


class SharedBridgeChecks(unittest.TestCase):
    def test_two_crossings_and_shared_deck_endpoint_use_one_surface(self):
        sources = controls()
        (deck, ground), report = build(sources)
        self.assertTrue(report["valid"], report)
        self.assertGreaterEqual(report["clearance"]["minimum_clearance_m"], 4.5)
        shared = [p for p in report["ports"] if p["point_enu_m"][:2] == [100, 0]]
        self.assertEqual(len(shared), 2)
        self.assertEqual(shared[0]["point_enu_m"], shared[1]["point_enu_m"])
        self.assertEqual(len(np.unique(deck.vertices[:, 2])), 1)
        for role, mesh, depth in (("deck", deck, 0.35), ("ground", ground, 0.25)):
            area = shapely.union_all(
                [corridor(s) for s in sources if s["role"] == role]
            ).area
            self.assertAlmostEqual(
                audit_closed(close_surface(mesh, depth)), area * depth, places=7
            )

    def test_mean_prior_disagreement_is_resolved_by_one_height_owner(self):
        sources = [
            source("a", [[-10, 0], [10, 0]]),
            source("b", [[0, -10], [0, 10]], heights=(1, 1)),
        ]
        prior, domain, _ = layout(sources)
        triangles = shapely.polygons(prior.vertices[prior.triangles, :2])
        self.assertTrue(shapely.coverage_is_valid(triangles))
        self.assertAlmostEqual(sum(p.area for p in triangles), domain.area)
        self.assertEqual(
            len(np.unique(prior.vertices[:, :2], axis=0)), len(prior.vertices)
        )
        self.assertGreater(
            shapely.intersection(corridor(sources[0]), corridor(sources[1])).area, 0
        )

    def test_fixed_dem_ports_fail_locally_and_retain_checked_ground(self):
        (deck, ground), report = build(controls(), port_lift_m=0)
        self.assertIsNone(deck)
        self.assertIsNotNone(ground)
        self.assertFalse(report["valid"])
        self.assertEqual(report["reason"], "deck_port_height_budget")
        (_, _), failed = build(controls(), port_lift_m=0.1)
        self.assertEqual(failed["reason"], "deck_port_height_budget")

    def test_port_limit_is_checked_at_every_source_endpoint(self):
        sources = controls()
        sources[1]["end_heights_m"] = [-2.0, 0.0]
        _, note = build(sources, port_lift_m=6)
        self.assertEqual(note["reason"], "deck_port_height_budget")
        self.assertGreater(note["maximum_deck_port_lift_m"], 6)

    def test_indoor_floor_is_explicitly_unresolved_and_never_given_zero_height(self):
        sources = controls() + [
            dict(
                source("indoor", [[20, 0], [50, 0]], role="deferred"),
                properties={"indoor": 1, "level": 1},
                omission_reason="indoor_surface_requires_buildings_contact",
            )
        ]
        meshes, note = build(sources)
        self.assertTrue(note["valid"])
        self.assertEqual(
            note["deferred_sources"],
            [
                {
                    "source_id": "indoor",
                    "reason": "indoor_surface_requires_buildings_contact",
                }
            ],
        )
        self.assertFalse(any(p["source_id"] == "indoor" for p in note["ports"]))
        _plan, binding = products(sources, meshes, note)
        self.assertEqual(len(binding["source_originals"]), 6)
        self.assertEqual(
            binding["pending_buildings_contacts"][0]["source_id"], "indoor"
        )
        self.assertFalse(any("indoor" in c["source_ids"] for c in binding["cells"]))

    def test_ambiguous_source_role_or_levels_do_not_receive_a_certificate(self):
        for field, value in (("layer", 0), ("layer", "1"), ("brunnel", "tunnel")):
            sources = controls()
            sources[0]["properties"][field] = value
            _, note = build(sources)
            self.assertFalse(note["valid"])
            self.assertEqual(note["reason"], "invalid_component_recipe")
        sources = controls()
        sources[2]["properties"]["subclass"] = "steps"
        self.assertFalse(build(sources)[1]["valid"])

    def test_bounds_and_duplicate_identity_precede_unbounded_layout(self):
        for sources in (
            (controls() * 14),
            [controls()[0], controls()[0], controls()[2]],
        ):
            self.assertFalse(build(sources)[1]["valid"])
        for argument in (
            {"port_lift_m": np.nan},
            {"ground_change_m": -1},
            {"seconds": 0},
        ):
            self.assertFalse(build(controls(), **argument)[1]["valid"])

    def test_pinned_real_component_preserves_all_sources_and_complete_coverage(self):
        from shared_bridge_experiment import source_usage

        self.assertEqual(hashlib.sha256(FIXTURE.read_bytes()).hexdigest(), PIN)
        original = json.loads(FIXTURE.read_text())
        sources = copy.deepcopy(original["sources"])
        self.assertEqual(len(sources), 33)
        self.assertEqual(len(set(original["all_axis_source_ids"])), 793)
        self.assertEqual(len(original["deferred_source_ids"]), 44)
        for method in ("cone", "polygon"):
            meshes, note = build(sources, method=method)
            self.assertTrue(note["valid"], note)
            self.assertEqual(note["unique_used_sources"], 23)
            self.assertEqual(len(note["deferred_sources"]), 10)
            self.assertFalse(note["all_source_contacts_resolved"])
            usage = source_usage(original, meshes, note)
            self.assertEqual(len(usage), 837)
            self.assertEqual(len({u["source_id"] for u in usage}), 837)
            self.assertEqual(sum(u["status"] == "used" for u in usage), 23)
            self.assertTrue(all(u["status"] == "used" or u["reason"] for u in usage))
            for role, mesh, depth in (
                ("deck", meshes[0], 0.35),
                ("ground", meshes[1], 0.25),
            ):
                area = shapely.union_all(
                    [corridor(s) for s in sources if s["role"] == role]
                ).area
                self.assertAlmostEqual(
                    audit_closed(close_surface(mesh, depth)), area * depth, places=6
                )
            gradients = normal_gradients(
                meshes[1].vertices[:, :2], meshes[1].triangles, meshes[1].vertices[:, 2]
            )
            self.assertLessEqual(
                np.linalg.norm(gradients, axis=1).max(), 0.025 * (1 + 1e-7)
            )
            self.assertLessEqual(note["maximum_deck_port_lift_m"], 8)
            self.assertGreaterEqual(note["clearance"]["minimum_clearance_m"], 4.5)
            self.assertLessEqual(note["ground_fit"]["maximum_height_change_m"], 2)
            plan, binding = products(sources, meshes, note)
            self.assertEqual(
                {s["source_id"] for s in binding["source_originals"]},
                {s["source_id"] for s in sources},
            )
            owners = {sid for cell in binding["cells"] for sid in cell["source_ids"]}
            self.assertEqual(
                owners, {s["source_id"] for s in sources if s["role"] != "deferred"}
            )
            ground_point = note["clearance"]["witness_xy"]
            sample = SurfaceSampler(meshes[1]).sample([ground_point])
            footprint = BuildingFootprint(
                "under",
                shapely.box(
                    *(np.asarray(ground_point) - 0.01),
                    *(np.asarray(ground_point) + 0.01),
                ),
                HeightInterval(
                    float(sample.points_enu_m[0, 2]),
                    float(sample.points_enu_m[0, 2] + 1),
                ),
            )
            structures = [
                r
                for r in plan.reservations
                if r.purpose == "transport_structure"
                and r.heights.lower_m > note["deck_height_m"] - 0.36
            ]
            self.assertFalse(any(r.overlaps(footprint) for r in structures))
        self.assertEqual(sources, original["sources"])


if __name__ == "__main__":
    unittest.main()
