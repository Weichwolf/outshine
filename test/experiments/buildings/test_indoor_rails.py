"""Rail clipping and actual hollow-section material geometry controls."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from indoor_rail_checks import RailAudit, covered, sweep_envelopes
from indoor_rail_clearance import RailClearance
from indoor_rail_geometry import hollow_sweep
from indoor_rail_instances import materialize
from indoor_rail_recipe import RailLimits
from indoor_rail_supports import SupportLimits
from outshine_ref.contracts import HeightInterval, LocalFrame, Reservation


class RailPrimitives(unittest.TestCase):
    def test_invalid_frames_dimensions_capacity_and_overlapping_thick_paths(self):
        for call in (
            lambda: RailLimits(wall_m=0.03),
            lambda: SupportLimits(cap_fraction=0.6),
            lambda: RailClearance((), 0.04, 0.002, max_tests=-1),
            lambda: hollow_sweep([(0, 0), (2, 0)], 0.04, 0.002, np.diag([1, 1, 2])),
            lambda: hollow_sweep(
                [(0, 0), (2, 0), (2, 0.2), (-1, 0.2), (-1, 0.02), (1, 0.02)],
                0.04,
                0.002,
            ),
            lambda: materialize({}, np.c_[np.diag([-1, 1, 1]), np.zeros(3)]),
        ):
            with self.assertRaises(ValueError):
                call()

    def test_actual_crossbar_blocks_port_even_with_a_closed_valid_mesh(self):
        reservation = Reservation(
            "port",
            shapely.box(0, -1, 2, 1),
            HeightInterval(0, 2.1),
            "traffic_clearance",
        )
        mesh, _ = hollow_sweep([(-1, 0), (3, 0)], 0.04, 0.002, origin=(0, 0, 1))
        record = {"mesh": mesh, "world_path": np.array([[-1, 0, 1], [3, 0, 1]])}
        with self.assertRaisesRegex(ValueError, "rail_material_blocks_traffic:port"):
            RailAudit((reservation,)).clear(sweep_envelopes(record)[0])

    def test_height_band_clipping_retains_separation_and_rejects_source_gap(self):
        vertices = np.array([[0, -0.02, 0], [0, 0.02, 0], [5, -0.02, 5], [5, 0.02, 5]])
        row = Reservation(
            "above-start",
            shapely.box(0, -1, 1, 1),
            HeightInterval(4, 5),
            "traffic_clearance",
        )
        RailAudit((row,)).clear(vertices)
        domain = shapely.box(-1, -1, 6, 1)
        with self.assertRaisesRegex(
            ValueError, "rail_material_outside_source_footprint"
        ):
            covered(
                vertices, [(0, 2, domain), (2, 3, shapely.Polygon()), (3, 5, domain)]
            )

    def test_straight_bent_and_periodic_hollow_material_volume(self):
        for path, closed in (
            ([(0, 0), (2, 0)], False),
            ([(0, 0), (2, 0), (3, 1), (5, 1)], False),
            ([(0, 0), (3, 0), (3, 2), (0, 2)], True),
        ):
            mesh, audit = hollow_sweep(path, 0.04, 0.002, closed=closed)
            self.assertGreater(audit["signed_volume_m3"], 0)
            self.assertLessEqual(
                audit["volume_error_m3"], audit["volume_arithmetic_bound_m3"]
            )
            self.assertEqual(len(mesh["faces"]), audit["triangles"])

    def test_reflected_plane_keeps_actual_outward_orientation(self):
        basis = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]])
        _, audit = hollow_sweep(
            [(0, 0), (2, 0), (3, 1)], 0.04, 0.002, basis, (4500, -400, 160)
        )
        self.assertGreater(audit["signed_volume_m3"], 0)
        self.assertLessEqual(
            audit["volume_error_m3"], audit["volume_arithmetic_bound_m3"]
        )

    def test_traffic_clip_preserves_true_holes_and_height_separation(self):
        polygon = shapely.box(0, 0, 4, 4).difference(shapely.box(1, 1, 3, 3))
        row = Reservation("way", polygon, HeightInterval(0, 2.1), "traffic_clearance")
        index = RailClearance((row,), 0.04, 0.002)
        pieces = index.clip([[-1, 2, 1], [5, 2, 1]])
        self.assertEqual(len(pieces), 3)
        self.assertGreater(pieces[1][0, 0], 1)
        self.assertLess(pieces[1][-1, 0], 3)
        self.assertEqual(len(index.clip([[-1, 2, 3], [5, 2, 3]])), 1)
        self.assertEqual(index.blocked([2, 2, -1], [2, 2, 3]), [])
        self.assertEqual(len(index.blocked([0.5, 0.5, -1], [0.5, 0.5, 3])), 1)


class ActualRailings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from compare_indoor_floors import DATA
        from compare_indoor_rails import prepare
        from indoor_rails import compose_rails

        cls.context, cls.batch, _ = prepare(DATA)
        cls.eager = compose_rails(
            cls.batch, cls.context.floors["reservations"], method="eager"
        )
        cls.shared = compose_rails(cls.batch, cls.context.floors["reservations"])

    def test_six_whole_source_owned_cases_with_unchanged_base_and_complete_aliases(
        self,
    ):
        from compare_indoor_stair_batch import signature
        from indoor_rails import compose_rails

        baseline = signature(self.batch)
        result = compose_rails(self.batch, self.context.floors["reservations"])
        self.assertEqual(signature(self.batch), baseline)
        self.assertEqual(len(result["ledger"]), 6)
        self.assertTrue(all(r["status"] == "candidate" for r in result["ledger"]))
        self.assertEqual(len(result["handrails"]), 12)
        self.assertEqual(len(result["opening_ledger"]), 6)
        self.assertTrue(
            all(r["status"] == "candidate" for r in result["opening_ledger"])
        )
        self.assertEqual(len({s for r in result["ledger"] for s in r["source_ids"]}), 7)
        self.assertEqual(self.context.old_ready_count, 30)
        self.assertEqual(len(self.context.floors["reservations"]), 73)
        self.assertEqual(sum(s["final_port_tests"] for s in self.batch["stairs"]), 12)

    def test_independent_geometry_material_and_complete_source_traffic_envelopes(self):
        from compare_indoor_rails import equivalent
        from indoor_rail_checks import qualify_rails

        agreement = equivalent(self.eager, self.shared)
        self.assertGreater(agreement["support_pairs"], 0)
        self.assertEqual(
            qualify_rails(self.eager, self.batch, self.context.floors),
            qualify_rails(self.shared, self.batch, self.context.floors),
        )
        self.assertEqual(self.shared["work"]["support_meshes_built"], 2)
        self.assertGreater(self.eager["work"]["support_meshes_built"], 2)
        self.assertLess(
            self.shared["work"]["mesh_payload_bytes"],
            self.eager["work"]["mesh_payload_bytes"],
        )

    def test_wrong_frame_rejected_before_mesh_and_blocked_flight_stays_local(self):
        from compare_indoor_stair_batch import signature
        from indoor_rails import compose_rails

        with patch("indoor_rails.handrails") as generate:
            with self.assertRaisesRegex(ValueError, "rail_common_frame_mismatch"):
                compose_rails(
                    self.batch | {"frame": LocalFrame("wrong")},
                    self.context.floors["reservations"],
                )
            generate.assert_not_called()
        baseline = signature(self.batch)
        stair = self.batch["stairs"][0]
        block = Reservation(
            "new-crossing",
            stair["footprint"].buffer(1),
            HeightInterval(
                stair["end_profiles"][0]["z_m"] - 1,
                stair["end_profiles"][-1]["z_m"] + 3,
            ),
            "traffic_clearance",
        )
        result = compose_rails(
            self.batch, (*self.context.floors["reservations"], block)
        )
        selected = next(
            r for r in result["ledger"] if r["source_id"] == stair["source_id"]
        )
        self.assertEqual(selected["reason"], "required_handrail_flight_clipped")
        self.assertTrue(any(r["status"] == "candidate" for r in result["ledger"]))
        self.assertEqual(signature(self.batch), baseline)

    def test_guard_posts_cannot_embed_through_the_actual_plate(self):
        from indoor_rail_guards import opening_guards

        result = opening_guards(
            self.batch,
            self.context.floors["reservations"],
            RailLimits(),
            support_limits=SupportLimits(embed_m=0.5),
        )
        self.assertFalse(result["guards"])
        self.assertTrue(
            all(
                r["reason"] == "guard_embed_below_actual_plate"
                for r in result["ledger"]
            )
        )


if __name__ == "__main__":
    unittest.main()
