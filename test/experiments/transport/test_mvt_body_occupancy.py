import copy
import unittest
from fractions import Fraction as F
from unittest.mock import patch

import numpy as np
import shapely
from mvt_body_occupancy import body_occupancy
from mvt_t_connection_inputs import inputs
from outshine_ref.contracts import BuildingFootprint, HeightInterval, LocalFrame
from stair_module_inputs import reference_profiles
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_inputs import volume
from stair_occupancy_pipeline import handoff
from test_mvt_native_body import construct


def sloped_pair():
    return {
        "body_vertices": np.array(
            [[0, 0, 5], [2, 0, 7], [0, 2, 5], [0, 0, 4], [2, 0, 6], [0, 2, 4]], float
        ),
        "top_faces": np.array([[0, 1, 2]]),
        "bottom_faces": np.array([[3, 4, 5]]),
        "face_sources": np.array([3]),
    }


def memberships(records):
    result = {}
    for row in records:
        result.setdefault(row["face"], {}).update(row["source_clearances_m"])
    return result


class ActualCapOccupancy(unittest.TestCase):
    def test_shared_physical_cap_preserves_all_sources_and_uses_maximum_profile(self):
        data = sloped_pair()
        profiles = {"path": 1, "road": 3}
        rows, owners, records, work = body_occupancy(
            data, tuple(profiles), profiles, owner_id="asset"
        )
        old, old_owners, expanded, old_work = body_occupancy(
            data, tuple(profiles), profiles, owner_id="asset", method="sources"
        )
        self.assertEqual(owners, (("path", "road"),))
        self.assertEqual(len(rows), 1)
        self.assertEqual(len(old), 2)
        self.assertEqual(memberships(records), memberships(expanded))
        self.assertEqual(work["plane_fits"], 2)
        self.assertEqual(old_work["plane_fits"], 4)
        for heights, area in [
            (HeightInterval(9, 10), 0.5),
            (HeightInterval(2, 4), 0),
            (HeightInterval(10, 11), 0),
        ]:
            footprint = BuildingFootprint(
                "probe", shapely.Polygon(data["body_vertices"][:3, :2]), heights
            )
            actual, _work = OccupancyQuery(rows).conflict(footprint)
            reference, _work = OccupancyQuery(old).conflict(footprint)
            self.assertEqual(actual.area, area)
            self.assertTrue(actual.equals(reference))
        self.assertEqual(sum(map(len, old_owners)), 2)

    def test_existing_pipeline_receives_actual_height_slice_and_retains_identity(self):
        data = sloped_pair()
        rows, *_ = body_occupancy(
            data, ("path", "road"), {"path": 1, "road": 3}, owner_id="asset"
        )
        original = BuildingFootprint(
            "building",
            shapely.Polygon(data["body_vertices"][:3, :2]),
            HeightInterval(9, 10),
            (2,),
            {"kind": "diagnostic", "provider": "retained"},
        )
        probes = tuple(
            BuildingFootprint(
                f"plant:{i}", shapely.box(x, 0.1, x + 0.1, 0.2), original.heights
            )
            for i, x in enumerate((0.1, 1.5))
        )
        changes, notes = adjusted(OccupancyQuery(rows), (original,))
        world, seen = handoff(LocalFrame("local"), (original,), rows, changes, probes)
        self.assertFalse(any(note.get("deferred") for note in notes))
        self.assertEqual(world.footprints[0].geometry.area, 1.5)
        self.assertEqual(world.footprints[0].key, original.key)
        self.assertEqual(world.footprints[0].attributes, original.attributes)
        self.assertTrue(OccupancyQuery(rows).conflict(probes[0])[0].is_empty)
        self.assertFalse(OccupancyQuery(rows).conflict(probes[1])[0].is_empty)
        # The retained building itself blocks the first probe after Transport.
        self.assertEqual(seen["probe_blocked"], (True, True))
        self.assertIs(seen["building_reservations"], world.transport.reservations)

    def test_missing_profile_unknown_mask_and_budget_fail_before_volume_fitting(self):
        for updates, profiles, limit in [
            ({}, {"path": 1}, 8192),
            ({"face_sources": np.array([4])}, {"path": 1, "road": 3}, 8192),
            ({}, {"path": 1, "road": 3}, 1),
        ]:
            data = sloped_pair()
            data.update(updates)
            with (
                patch(
                    "mvt_body_occupancy.volume",
                    side_effect=AssertionError("unexpected fit"),
                ),
                self.assertRaises(ValueError),
            ):
                body_occupancy(
                    data,
                    ("path", "road"),
                    profiles,
                    owner_id="asset",
                    method="sources",
                    max_volumes=limit,
                )

    def test_volume_does_not_follow_later_input_buffer_mutation(self):
        data = sloped_pair()
        rows, *_ = body_occupancy(
            data, ("path", "road"), {"path": 1, "road": 3}, owner_id="asset"
        )
        geometry, plane = rows[0].geometry.wkb, rows[0].upper_plane_m
        data["body_vertices"][:] = 100
        self.assertEqual(rows[0].geometry.wkb, geometry)
        self.assertEqual(rows[0].upper_plane_m, plane)


class NativeMvtOccupancy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, sources = inputs()
        import json

        from mvt_t_connection_inputs import FIXTURE

        native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        _solver, cls.plan, _contacts, _heights, cls.original, *_ = construct(
            cls.case, native
        )
        profiles = reference_profiles()
        cls.ids = tuple(row["source_id"] for row in sources)
        cls.clearances = {
            row["source_id"]: profiles[row["kind"]]["clearanceM"] for row in sources
        }

    def test_smooth_reference_headroom_can_expose_a_band_reserved_by_the_actual_tread(
        self,
    ):
        data = self.original
        actual = data["body_vertices"][data["top_faces"]]
        reference = data["reference_vertices"][data["reference_faces"]]
        candidates = np.flatnonzero(data["face_step"] >= 0)
        face = max(
            candidates, key=lambda i: np.mean(actual[i, :, 2] - reference[i, :, 2])
        )
        inner = actual[face, :, :2].mean(axis=0) * 0.75 + actual[face, :, :2] * 0.25
        clearance = max(
            value
            for i, value in enumerate(self.clearances.values())
            if int(data["face_sources"][face]) & (1 << i)
        )
        reference_upper = (
            reference[face, :, 2].mean() * 0.75 + reference[face, :, 2] * 0.25
        ).max() + clearance
        actual_upper = actual[face, 0, 2] + clearance
        gap = actual_upper - reference_upper
        self.assertGreater(gap, 0.05)
        footprint = BuildingFootprint(
            "diagnostic-upper-band",
            shapely.Polygon(inner),
            HeightInterval(reference_upper + 0.25 * gap, reference_upper + 0.75 * gap),
        )
        rows, *_ = body_occupancy(data, self.ids, self.clearances, owner_id="actual")
        smooth = tuple(
            volume(
                f"smooth:{i}",
                data["body_vertices"][bottom],
                reference[i],
                max(
                    value
                    for bit, value in enumerate(self.clearances.values())
                    if int(data["face_sources"][i]) & (1 << bit)
                ),
            )[0]
            for i, bottom in enumerate(data["bottom_faces"])
        )
        occupied, _work = OccupancyQuery(rows).conflict(footprint)
        exposed, _work = OccupancyQuery(smooth).conflict(footprint)
        self.assertFalse(occupied.is_empty)
        self.assertAlmostEqual(occupied.area, footprint.geometry.area, delta=1e-12)
        self.assertTrue(exposed.is_empty)

    def test_every_decoded_cap_corner_is_covered_outward_with_original_source_masks(
        self,
    ):
        for cast in (False, True):
            data = copy.copy(self.original)
            if cast:
                data["body_vertices"] = data["body_vertices"].astype("f4").astype(float)
            rows, origins, records, work = body_occupancy(
                data, self.ids, self.clearances, owner_id="native-body"
            )
            self.assertEqual(len(rows), 196)
            self.assertEqual(work["unused_sources"], [])
            for i, (row, keys, record) in enumerate(
                zip(rows, origins, records, strict=True)
            ):
                expected = tuple(
                    key
                    for bit, key in enumerate(self.ids)
                    if int(data["face_sources"][i]) & (1 << bit)
                )
                self.assertEqual(keys, expected)
                self.assertEqual(record["face"], i)
                self.assertTrue(
                    row.geometry.equals_exact(
                        shapely.Polygon(
                            data["body_vertices"][data["top_faces"][i], :2]
                        ),
                        0,
                    )
                )
                for kind, delta, sign in [
                    ("bottom", 0, -1),
                    ("top", record["clearance_m"], 1),
                ]:
                    plane = row.lower_plane_m if kind == "bottom" else row.upper_plane_m
                    a, b, c = map(F, plane)
                    ox, oy = map(F, row.origin_xy_m)
                    intercept = a - b * ox - c * oy
                    for x, y, z in data["body_vertices"][data[kind + "_faces"][i]]:
                        padding = sign * (
                            intercept + b * F(x) + c * F(y) - F(z) - F(delta)
                        )
                        self.assertGreaterEqual(padding, 0)
                        self.assertLess(padding, F(1, 100000000))


if __name__ == "__main__":
    unittest.main()
