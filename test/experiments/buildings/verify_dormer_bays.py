"""Actual facade ownership, retained roof function and bounded bay counterexamples."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from dormer_bays import FacadeBays
from dormer_footprint import projected
from dormer_plan import plan
from dormer_sources import sources
from dormer_windows import DEPTH_M, aperture
from facade_parts import append
from provider_contract import attributes
from structural_shell_geometry import native
from tile_dormers import DormerFacades
from tile_roof_envelopes import choose
from verify_elevations import build, untimed
from verify_facade_frames import view_for


class AlignedDormers(DormerFacades):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, dormer_layout="facade", **kwargs)


class BaySelectionTests(unittest.TestCase):
    def setUp(self):
        self.frame = {
            "id": "source/facade",
            "a": [0.0, 0.0],
            "b": [20.0, 0.0],
            "width_m": 20.0,
            "columns": 8,
        }

    def test_nearest_axis_clamps_and_breaks_exact_midpoint_ties_to_the_left(self):
        for method in ("linear", "bisect"):
            bays = FacadeBays((self.frame,), method=method)
            for query, expected in (
                (-1e100, 0),
                (2.5, 0),
                (np.nextafter(2.5, np.inf), 1),
                (1e100, 7),
            ):
                self.assertEqual(
                    bays.nearest((0.0, 0.0), (20.0, 0.0), query)[1], expected
                )

    def test_bounded_selection_matches_exhaustive_control_at_all_cell_boundaries(self):
        frame = self.frame | {"columns": 257}
        first, second = (
            FacadeBays((frame,), method=method) for method in ("linear", "bisect")
        )
        positions = np.linspace(-2, 22, 2049)
        for x in positions:
            self.assertEqual(
                first.nearest(frame["a"], frame["b"], x),
                second.nearest(frame["a"], frame["b"], x),
            )

    def test_missing_reversed_duplicate_and_invalid_frames_are_not_invented(self):
        invalid = [
            (),
            (self.frame, self.frame),
            (self.frame | {"columns": 0},),
            (self.frame | {"columns": 4097},),
            (self.frame | {"columns": True},),
            (self.frame | {"width_m": np.inf},),
            (self.frame | {"b": [21.0, 0.0]},),
            (self.frame | {"a": [np.nan, 0.0]},),
        ]
        for frames in invalid:
            with self.subTest(frames=frames), self.assertRaises(ValueError):
                FacadeBays(frames)
        bays = FacadeBays((self.frame,))
        with self.assertRaisesRegex(ValueError, "no exact upper facade owner"):
            bays.nearest(self.frame["b"], self.frame["a"], 2.5)
        with self.assertRaisesRegex(ValueError, "Finite"):
            bays.nearest(self.frame["a"], self.frame["b"], np.nan)


class ActualDormerBayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources()
        cls.original = copy.deepcopy(cls.rows)
        cls.before = [
            build(DormerFacades, row, neighbors, view_for(row))
            for row, neighbors in cls.rows
        ]
        cls.after = [
            build(AlignedDormers, row, neighbors, view_for(row))
            for row, neighbors in cls.rows
        ]

    def test_actual_upper_scoped_and_ordinary_window_axes_own_all_new_dormers(self):
        scoped, ordinary = 0, 0
        for (row, _), (_mesh, info, owner) in zip(self.rows, self.after, strict=True):
            asset = owner.base.dormer_assets[row["id"]]
            actual = {f["id"]: f for f in info["facade_plan"]}
            self.assertTrue(asset.frontages)
            self.assertFalse(owner.roof_staged_elevations)
            self.assertFalse(owner.base.dormer_rejected)
            for dormer in asset.dormers:
                identifier, column = dormer.identifier.rsplit("/dormer/bay/", 1)
                frame = actual[identifier]
                tangent = (np.asarray(frame["b"]) - frame["a"]) / frame["width_m"]
                centre = (np.asarray(dormer.a) + dormer.b) / 2
                along = (centre - frame["a"]) @ tangent
                self.assertAlmostEqual(
                    along,
                    (int(column) + 0.5) * frame["width_m"] / frame["columns"],
                    delta=1e-7,
                )
                self.assertAlmostEqual(
                    (centre - frame["a"]) @ np.array([-tangent[1], tangent[0]]),
                    0.05,
                    delta=1e-7,
                )
                scoped += frame.get("architecture_scope") == "top"
                ordinary += "architecture_scope" not in frame
        self.assertGreater(scoped, 0)
        self.assertGreater(ordinary, 0)
        self.assertEqual(self.rows, self.original)

    def test_original_entrances_and_actual_full_facade_programmes_remain_exact(self):
        for (_, old, old_owner), (_, new, owner) in zip(
            self.before, self.after, strict=True
        ):
            for key in (
                "facade_plan",
                "facade_architecture_plan",
                "entrance_plan",
                "shopfront_plan",
                "balcony_plan",
                "window_bay_plan",
                "facade_programme",
            ):
                self.assertEqual(untimed(old.get(key)), untimed(new.get(key)), key)
            self.assertEqual(old_owner.entrances.planned, owner.entrances.planned)
            self.assertEqual(old_owner.entrances.rejected, owner.entrances.rejected)

    def test_real_union_and_pockets_keep_independent_volume_holes_height_and_seams(
        self,
    ):
        for (row, _), (_, _info, owner) in zip(self.rows, self.after, strict=True):
            asset = owner.base.dormer_assets[row["id"]]
            expected = asset.original.volume_m3 + sum(
                p.added_volume_m3 for p in asset.dormers
            )
            self.assertAlmostEqual(asset.full.physical.volume_m3, expected, delta=1e-7)
            near = asset.near_products()
            removed = sum(
                aperture(p)[1] * aperture(p)[2] * DEPTH_M for p in asset.dormers
            )
            self.assertAlmostEqual(
                near.physical.volume_m3, expected - removed, delta=1e-7
            )
            self.assertIs(near, asset.near_products())
            regions = [shapely.from_wkb(p.footprint_wkb) for p in asset.dormers]
            for index, region in enumerate(regions):
                self.assertTrue(row["polygon"].covers(region))
                self.assertTrue(
                    all(region.distance(other) >= 0.5 for other in regions[index + 1 :])
                )
            for product in (asset.full, near):
                solid = native(product.physical.mesh, product.physical.vertex_roots)
                joined = native(append(product.body, [product.skin]))
                self.assertLess(
                    (solid - joined).volume() + (joined - solid).volume(), 1e-7
                )
                self.assertLess(
                    row["polygon"]
                    .symmetric_difference(projected(product.physical.mesh))
                    .area,
                    1e-7,
                )
                self.assertEqual(
                    float(product.physical.mesh["vertices"][:, 2].max()),
                    attributes(row["properties"])["buildingHeight"],
                )

    def test_exhaustive_and_bounded_selection_produce_identical_complete_plans(self):
        for (row, _), (_, _info, owner) in zip(self.rows, self.after, strict=True):
            source = owner.rows[0]
            params = attributes(source["properties"])
            asset = owner.base.dormer_assets[row["id"]]
            outputs = [
                plan(
                    source,
                    asset.original,
                    choose(source, params),
                    frontages=asset.frontages,
                    bay_method=method,
                    clear=lambda geometry, low, high, owner=owner, source=source: (
                        owner.entrances.clear(source, geometry, high, low_m=low)
                    ),
                )
                for method in ("linear", "bisect")
            ]
            self.assertEqual(outputs[0], outputs[1])
            self.assertEqual(outputs[0][0], asset.dormers)
            self.assertEqual(outputs[0][1], asset.rejected)

    def test_prepared_logical_axes_and_roof_are_reused_without_replanning(self):
        for (row, _), (_, _, owner) in zip(self.rows, self.after, strict=True):
            source = owner.rows[0]
            params = attributes(source["properties"])
            asset = owner.base.dormer_assets[row["id"]]
            owner.base.view = replace(view_for(row), width=1280, height=720)
            with patch.object(
                owner.base,
                "prepare_asset",
                side_effect=AssertionError("prepared roof rebuilt"),
            ):
                self.assertIs(
                    owner.base.asset(source, params, choose(source, params)), asset
                )

    def test_unresolved_new_frames_retain_the_complete_previous_dormer_product(self):
        class MissingFrames(AlignedDormers):
            def stage_roof_facades(self, row, parameters):
                super().stage_roof_facades(row, parameters)
                return ()

        row, neighbors = self.rows[1]
        mesh, info, owner = build(MissingFrames, row, neighbors, view_for(row))
        previous, old_info, _ = self.before[1]
        for key, value in previous.items():
            np.testing.assert_array_equal(mesh[key], value, strict=True)
        self.assertEqual(info["roof_dormer_plan"], old_info["roof_dormer_plan"])
        self.assertTrue(
            owner.base.dormer_rejected[0]["reason"].startswith("aligned fallback:")
        )
        self.assertFalse(owner.roof_staged_elevations)

    def test_later_inconsistent_facade_binding_cannot_admit_a_different_roof_grid(self):
        class DifferentFrames(AlignedDormers):
            def stage_roof_facades(self, row, parameters):
                frames = super().stage_roof_facades(row, parameters)
                return (frames[0] | {"columns": frames[0]["columns"] + 1}, *frames[1:])

        row, neighbors = self.rows[1]
        with self.assertRaisesRegex(
            ValueError, "Dormer axes differ from the final bound facade"
        ):
            build(DifferentFrames, row, neighbors, view_for(row))

    def test_failed_physical_facade_does_not_retain_staged_planning_state(self):
        row, neighbors = self.rows[1]
        owner = AlignedDormers([row, *neighbors], view_for(row))
        source = owner.rows[0]
        with (
            patch(
                "tile_elevations.ElevationFacades.surface",
                side_effect=ValueError("invalid physical facade"),
            ),
            self.assertRaisesRegex(
                ValueError, "Dormer axes differ from the final bound facade"
            ),
        ):
            owner(source, attributes(source["properties"]), 12)
        self.assertFalse(owner.roof_staged_elevations)


if __name__ == "__main__":
    unittest.main()
