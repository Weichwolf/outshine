"""Independent surface measures and C/T/S transport, not image-acceptance oracles."""

import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from compare_directed_groups import kept_visibility
from directed_group_transport import densities, prepare_view, render_groups
from directed_surface_groups import compile_groups
from packed_normals import pack
from parent_surface_inputs import ParentInputs, open_parent
from source_surface_patches import SurfacePatch, original_patches, shape_patches
from urban_parent_layer import compose


def plate(domain, origin=(0, 0, 0.5), axes=((1, 0, 0), (0, 1, 0))):
    axes = np.asarray(axes, float)
    normal = np.cross(*axes)
    normal /= np.linalg.norm(normal)
    return SurfacePatch(domain, np.asarray(origin, float), axes, normal, 9)


class SurfaceMeasuresTest(unittest.TestCase):
    def test_unrepresentable_coordinates_fail_before_integer_conversion(self):
        for x in [1e25, -1e25, np.inf]:
            with self.subTest(x=x), self.assertRaisesRegex(ValueError, "int32"):
                compile_groups([plate(shapely.box(0, 0, 1, 1), (x, 0, 0))], [1] * 3)

    def test_total_clip_budget_is_checked_before_any_clipping(self):
        surfaces = [plate(shapely.box(0, 0, 1025, 512))] * 2
        with patch("directed_surface_groups.clipped_area") as clip:
            with self.assertRaisesRegex(ValueError, "bounded total"):
                compile_groups(surfaces, [1, 1, 1])
            clip.assert_not_called()

    def test_plate_area_is_partitioned_once_even_on_a_height_boundary(self):
        for z in [0.5, 1.0]:
            field, report = compile_groups(
                [plate(shapely.box(0, 0, 2, 2), (0, 0, z))], [1, 1, 1]
            )
            self.assertEqual(report["input_surface_area_m2"], 4)
            self.assertEqual(field["area_m2"].sum(), 4)
            self.assertEqual(len(field["cells"]), 4)
            np.testing.assert_array_equal(field["cells"][:, 2], [int(z)] * 4)

    def test_sloped_surface_uses_true_area_and_height_clipping(self):
        field, report = compile_groups(
            [plate(shapely.box(0, 0, 2, 1), (0, 0, 0), ((1, 0, 1), (0, 1, 0)))],
            [1, 1, 1],
        )
        self.assertAlmostEqual(report["stored_surface_area_m2"], 2 * np.sqrt(2))
        self.assertEqual(len(field["cells"]), 2)
        self.assertEqual(set(map(tuple, field["cells"])), {(0, 0, 0), (1, 0, 1)})

    def test_holes_do_not_acquire_surface_mass(self):
        polygon = shapely.Polygon(
            [(0, 0), (3, 0), (3, 3), (0, 3)], [[(1, 1), (2, 1), (2, 2), (1, 2)]]
        )
        field, _ = compile_groups([plate(polygon)], [1, 1, 1])
        self.assertEqual(field["area_m2"].sum(), 8)
        self.assertNotIn((1, 1, 0), set(map(tuple, field["cells"])))

    def test_original_flat_box_has_four_walls_and_one_roof(self):
        shape = {
            "ring": [[0, 0], [2, 0], [2, 2], [0, 2]],
            "holes": [],
            "roof_kind": 0,
            "rise_m": 0,
            "seat_m": 0,
            "foot_m": 0,
            "eaves_m": 2,
            "sole_m": 0,
        }
        surface = {
            "origin": [0, 0, 0],
            "east": [1, 0, 0],
            "north": [0, 1, 0],
            "up": [0, 0, 1],
        }
        patches = shape_patches(surface, shape, 17, np.eye(3), np.zeros(3))
        area = sum(p.domain.area * np.linalg.norm(np.cross(*p.axes)) for p in patches)
        self.assertEqual(len(patches), 5)
        self.assertEqual(area, 20)
        self.assertTrue(all(p.source == 17 for p in patches))

    def test_opposing_normal_lobes_are_not_replaced_by_their_mean(self):
        arrays = {
            "cells": np.array([[0, 0, 0]]),
            "pitch_m": np.ones(3),
            "offsets": np.array([0, 2]),
            "normal_words": pack(np.array([[1, 0, 0], [0, 0, 1]])),
            "normal_ids": np.array([0, 1]),
            "area_m2": np.ones(2),
        }
        direction = np.array([[1, 0, -1]]) / np.sqrt(2)
        value = densities(arrays, direction)[0][0]
        self.assertAlmostEqual(value, np.sqrt(2), places=4)
        self.assertGreater(value, 1.4)

    def test_unsupported_part_keeps_the_entire_source_out_of_the_field(self):
        valid = {
            "ring": [[0, 0], [2, 0], [2, 2], [0, 2]],
            "holes": [],
            "roof_kind": 0,
            "rise_m": 0,
            "seat_m": 0,
            "foot_m": 0,
            "eaves_m": 2,
            "sole_m": 0,
        }
        surface = {
            "origin": [0, 0, 0],
            "east": [1, 0, 0],
            "north": [0, 1, 0],
            "up": [0, 0, 1],
            "shapes": [valid],
        }
        parent = SimpleNamespace(
            plans={
                "summaries": [
                    {
                        "supported": True,
                        "frame": [
                            surface[k] for k in ("origin", "east", "north", "up")
                        ],
                    }
                ]
            },
            iter_forms=lambda _: iter(
                [
                    (
                        1,
                        [3, 4],
                        [
                            surface | {"shapes": [valid, valid | {"roof_kind": 6}]},
                            surface,
                        ],
                    )
                ]
            ),
        )
        patches, _, _, accepted, needs = original_patches(parent, {1})
        self.assertEqual(len(patches), 5)
        np.testing.assert_array_equal(accepted, [4])
        self.assertEqual([p.source for p in patches], [4] * 5)
        self.assertEqual([row["source"] for row in needs], [3])


class OriginalSurfaceMeasureTest(unittest.TestCase):
    def test_original_field_build_reads_only_the_plan_index_and_selected_forms(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        original_member = ParentInputs.member
        read = []

        def member(parent, name):
            self.assertIn(name, {parent.metadata["index"], parent.forms[33]})
            read.append(name)
            return original_member(parent, name)

        with patch.object(ParentInputs, "member", member), open_parent(path) as parent:
            patches, _, _, accepted, needs = original_patches(parent, {33})
            source_ids = np.flatnonzero(parent.cells == 33)
            unknown = np.array([row["source"] for row in needs])
            np.testing.assert_array_equal(np.sort(np.r_[accepted, unknown]), source_ids)
            self.assertEqual(len(accepted), 575)
            self.assertEqual(len(needs), 1)
            self.assertIn("hip facets", needs[0]["reason"])
            field, report = compile_groups(patches, [64, 64, 4])
            self.assertAlmostEqual(
                report["stored_surface_area_m2"],
                report["input_surface_area_m2"],
                places=7,
            )
            self.assertEqual(len(field["cells"]), 126)
            self.assertEqual(parent.form_reads, 1)
            self.assertEqual(len(read), 2)


class GroupTransportTest(unittest.TestCase):
    def setUp(self):
        self.field = {
            "cells": np.array([[0, 0, 0]]),
            "pitch_m": np.array([2.0, 2.0, 2.0]),
        }
        self.view = {
            "extinction": np.array([0.5]),
            "source_rgb": np.array([[0.5, 1, 1.5]]),
            "light_extinction": np.zeros(1),
        }
        self.origins = np.array([[1.0, 1.0, 4.0]])
        self.directions = np.array([[0.0, 0.0, -1.0]])
        self.profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 1,
            "ambient": 0,
        }

    def test_homogeneous_slab_matches_independent_exponential_solution(self):
        c, t, s, _ = render_groups(
            self.field,
            self.view,
            self.origins,
            self.directions,
            np.array([4.0]),
            np.array([[1, 1, -0.001]]),
            self.profile,
        )
        np.testing.assert_allclose(t, [np.exp(-1)])
        np.testing.assert_allclose(c, (1 - np.exp(-1)) * np.array([[1, 2, 3]]))
        np.testing.assert_array_equal(s, [1])
        result = compose(c, t, s, np.ones((1, 3)) * 2, np.ones((1, 3)) * 3)
        np.testing.assert_allclose(result, c + np.exp(-1) * 5)

    def test_retained_caster_blocks_field_illumination(self):
        field, _ = compile_groups([plate(shapely.box(0, 0, 2, 2))], [2, 2, 2])
        eye = np.array([1, 1, 4.0])
        unblocked, _ = prepare_view(field, eye, self.profile)
        blocked, work = prepare_view(
            field, eye, self.profile, lambda centres: np.zeros(len(centres))
        )
        self.assertGreater(unblocked["source_rgb"].sum(), 0)
        np.testing.assert_array_equal(blocked["source_rgb"], [[0, 0, 0]])
        np.testing.assert_array_equal(blocked["extinction"], unblocked["extinction"])
        self.assertEqual(work["retained_caster_samples"], 1)

    def test_invalid_retained_caster_visibility_is_rejected(self):
        field, _ = compile_groups([plate(shapely.box(0, 0, 2, 2))], [2, 2, 2])
        for value in [np.array([np.nan]), np.array([2]), np.array([1, 0])]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                prepare_view(
                    field,
                    np.array([1, 1, 4.0]),
                    self.profile,
                    lambda _, value=value: value,
                )

    def test_retained_shadow_query_with_no_hits_keeps_the_light(self):
        ray = SimpleNamespace(
            intersects_location=lambda *_args, **_kwargs: (
                np.array([]),
                np.array([], int),
                np.array([], int),
            )
        )
        value = kept_visibility(
            None, {"ray": ray}, self.origins, np.zeros((1, 3)), self.profile
        )
        np.testing.assert_array_equal(value, [1])

    def test_retained_shadow_beyond_point_emitter_is_not_a_caster(self):
        ray = SimpleNamespace(
            intersects_location=lambda *_args, **_kwargs: (
                np.array([[1, 1, 20]]),
                np.array([0]),
                np.array([0]),
            )
        )
        light = {"kind": "point", "position": [1, 1, 10], "intensity": 1}
        value = kept_visibility(
            None, {"ray": ray}, self.origins, np.zeros((1, 3)), light
        )
        np.testing.assert_array_equal(value, [1])

    def test_retained_stop_before_group_keeps_the_background(self):
        c, t, s, _ = render_groups(
            self.field,
            self.view,
            self.origins,
            self.directions,
            np.array([1.0]),
            np.array([[1, 1, 3]]),
            self.profile,
        )
        np.testing.assert_array_equal(c, [[0, 0, 0]])
        np.testing.assert_array_equal(t, [1])
        np.testing.assert_array_equal(s, [1])

    def test_receiver_shadow_uses_a_separate_field_path(self):
        self.view["light_extinction"][:] = 0.25
        c, t, s, _ = render_groups(
            self.field,
            self.view,
            self.origins,
            self.directions,
            np.array([4.0]),
            np.array([[1, 1, -0.001]]),
            self.profile,
        )
        np.testing.assert_allclose(s, [np.exp(-0.5)])
        np.testing.assert_allclose(t, [np.exp(-1)])
        self.assertGreater(c.min(), 0)
