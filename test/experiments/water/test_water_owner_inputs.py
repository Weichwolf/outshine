"""Input arrival and source-layer invalidation before any fine-water work."""

import dataclasses
import unittest

import numpy as np
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_water_owner_inputs import course_contents, layer_receipts
from water_owner_inputs import LAYERS, LayerReceipt, fixed_owner_inputs

CELL = (4, 5, 5)
EPOCH = ("provider", "revision-r1")


def receipts():
    return {
        ((4, x, y), layer): LayerReceipt(
            (4, x, y), layer, EPOCH, f"{layer}:{x}:{y}", True
        )
        for x in range(3, 8)
        for y in range(3, 8)
        for layer in LAYERS
    }


def plan(items, **options):
    return fixed_owner_inputs(
        CELL, items, epoch=EPOCH, model_revision="river-fixed-domain-v1", **options
    )


class WaterOwnerInputs(unittest.TestCase):
    def test_residence_window_and_arrival_order_do_not_change_fixed_dependencies(self):
        all_receipts = receipts()
        initial = plan(all_receipts)
        fixed = {key: all_receipts[key] for key in reversed(initial.required)}
        second = plan(fixed)
        self.assertEqual(initial, second)
        self.assertEqual(initial.state, "ready")
        self.assertEqual(len(initial.tiles), 9)
        self.assertEqual(len(initial.required), 18)
        self.assertEqual(initial.work["receipt_lookups"], 18)

    def test_missing_and_failed_profile_layer_never_publish_a_partial_owner(self):
        items = receipts()
        missing = (CELL, "water_lines")
        original = items.pop(missing)
        first = plan(items)
        self.assertEqual(first.state, "pending_sources")
        self.assertEqual(first.missing, (missing,))
        self.assertIsNone(first.version)
        items[missing] = dataclasses.replace(original, complete=False)
        self.assertEqual(first, plan(items))
        items[missing] = original
        self.assertEqual(plan(items).state, "ready")

    def test_polygon_profile_and_recipe_changes_invalidate_only_the_declared_stencil(
        self,
    ):
        items = receipts()
        initial = plan(items)
        for layer in LAYERS:
            changed = dict(items)
            key = (CELL, layer)
            changed[key] = dataclasses.replace(changed[key], digest="changed")
            self.assertNotEqual(plan(changed).version, initial.version)
        irrelevant = ((4, 3, 3), "water_lines")
        changed = dict(items)
        changed[irrelevant] = dataclasses.replace(
            changed[irrelevant], digest="outside-change"
        )
        self.assertEqual(plan(changed), initial)
        recipe = fixed_owner_inputs(
            CELL, items, epoch=EPOCH, model_revision="other-recipe"
        )
        self.assertNotEqual(recipe.version, initial.version)
        other_centre = fixed_owner_inputs(
            (4, 6, 5), items, epoch=EPOCH, model_revision="river-fixed-domain-v1"
        )
        self.assertNotEqual(other_centre.version, initial.version)

    def test_valid_empty_layers_and_every_arrival_prefix_are_explicit(self):
        items = receipts()
        required = plan(items).required
        arriving = {}
        for key in required:
            p = plan(arriving)
            self.assertEqual(p.state, "pending_sources")
            self.assertIsNone(p.version)
            arriving[key] = dataclasses.replace(
                items[key], digest="explicit-empty-layer"
            )
        result = plan(arriving)
        self.assertEqual(result.state, "ready")
        self.assertEqual(result.missing, ())

    def test_metadata_lookup_does_not_scan_or_copy_the_residence_map(self):
        class LookupOnly:
            def __init__(self, data):
                self.data, self.calls = data, []

            def get(self, key):
                self.calls.append(key)
                return self.data.get(key)

            def __iter__(self):
                raise AssertionError("residence map must not be traversed")

        index = LookupOnly(receipts())
        result = plan(index)
        self.assertEqual(tuple(index.calls), result.required)
        self.assertEqual(len(index.calls), 18)

    def test_source_identity_epoch_and_work_bounds_refuse(self):
        items = receipts()
        key = (CELL, "water_lines")
        for field, value in (
            ("tile", (4, 6, 5)),
            ("epoch", ("provider", "r2")),
            ("layer", "water_polygons"),
            ("digest", ""),
            ("complete", 1),
        ):
            changed = dict(items)
            changed[key] = dataclasses.replace(changed[key], **{field: value})
            with self.assertRaisesRegex(ValueError, "receipt"):
                plan(changed)
        for radius in (0, 4, True):
            with self.assertRaisesRegex(ValueError, "bounded fixed radius"):
                plan(items, radius=radius)
        maximum = fixed_owner_inputs(
            (5, 10, 10), {}, epoch=EPOCH, model_revision="r1", radius=3
        )
        self.assertEqual(len(maximum.tiles), 49)
        self.assertEqual(maximum.work["receipt_lookups"], 98)

    def test_projection_seams_are_not_silently_wrapped_or_declared_empty(self):
        for cell in ((4, 0, 5), (4, 5, 0), (4, 15, 5), (4, 5, 15)):
            result = fixed_owner_inputs(cell, {}, epoch=EPOCH, model_revision="r1")
            self.assertEqual(result.state, "needs_projection_contacts")
            self.assertIsNone(result.version)
            self.assertEqual(result.work["receipt_lookups"], 0)


class WaterOwnerSourcePayloads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, raw, _indices = observed_snapshots(DEFAULT)
        cls.snapshots = {
            tile: dataclasses.replace(s, complete=True) for tile, s in raw.items()
        }
        cls.epoch = tuple(
            next(iter(raw.values())).source[k] for k in ("provider", "revision")
        )
        cls.index = next(
            i for i, c in enumerate(cls.data["courses"]) if c["point_count"] >= 2
        )
        cls.tile = course_contents(cls.data, cls.index)[0]

    def test_actual_profile_geometry_levels_and_semantics_are_receipt_dependencies(
        self,
    ):
        before, _ = layer_receipts(self.data, self.snapshots, self.epoch)
        course = self.data["courses"][self.index]
        for field in ("levels", "points_lat_lon", "inputs"):
            changed = dict(self.data)
            if field == "inputs":
                changed[field] = [dict(info) for info in self.data[field]]
                changed[field][int(course["input"])]["bridge"] = not changed[field][
                    int(course["input"])
                ]["bridge"]
            else:
                changed[field] = self.data[field].copy()
                start = int(
                    course["first_level"]
                    if field == "levels"
                    else course["first_point"]
                )
                changed[field][start] += 1 if field == "levels" else 1e-6
            after, _ = layer_receipts(changed, self.snapshots, self.epoch)
            self.assertNotEqual(
                before[(self.tile, "water_lines")].digest,
                after[(self.tile, "water_lines")].digest,
                field,
            )
            self.assertEqual(
                before[(self.tile, "water_polygons")],
                after[(self.tile, "water_polygons")],
            )

    def test_native_course_table_permutation_keeps_receipts_and_semantic_order(self):
        before, order = layer_receipts(self.data, self.snapshots, self.epoch)
        changed = dict(self.data, courses=self.data["courses"][::-1])
        after, reversed_order = layer_receipts(
            changed, dict(reversed(list(self.snapshots.items()))), self.epoch
        )
        self.assertEqual(before, after)
        for tile in order:
            first = [course_contents(self.data, i)[1:] for i in order[tile]]
            second = [course_contents(changed, i)[1:] for i in reversed_order[tile]]
            self.assertEqual(first, second)
        np.testing.assert_array_equal(self.data["courses"], changed["courses"][::-1])


if __name__ == "__main__":
    unittest.main()
