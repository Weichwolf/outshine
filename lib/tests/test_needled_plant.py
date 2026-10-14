import json
import unittest

import numpy as np
from outshine_ref.vegetation import (
    AxisLevel,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
)


def plant(seed=13):
    wood = PlantPlan(
        seed,
        (
            AxisLevel(1, 2.8, 0.04, 0.01, bend=0.02, bend_variation=0.01),
            AxisLevel(12, 0.8, 0.004, 0.0012, whorl_size=3, last=0.85, tip_scale=0.25),
            AxisLevel(5, 0.2, 0.0007, 0.0003, last=0.85, tip_scale=0.6),
        ),
        (),
    )
    patterns = (
        None,
        NeedlePattern(spacing=0.08, group_size=2),
        NeedlePattern(spacing=0.017, group_size=5),
    )
    return NeedledPlant(wood, patterns, (Needle(length=0.055), Needle(length=0.07)))


class NeedledPlantTests(unittest.TestCase):
    def test_unretained_groups_preserve_identity_and_survive_later_queries(self):
        p, q = plant(), plant()
        path = (3, 2)
        original = p.foliage(path, 100000)
        saved = q.foliage(path, 100000, retain=False)
        for other in q.wood.paths():
            q.foliage(other, 100000, retain=False)
        self.assertFalse(q._groups)
        for key in original:
            np.testing.assert_array_equal(original[key], saved[key])
            self.assertFalse(saved[key].flags.writeable)
        later = q.foliage(path, 100000)
        self.assertIs(q.foliage(path, 100000, retain=False), later)
        self.assertEqual(len(q._groups), 1)
        self.assertEqual(p.recipe(), q.recipe())
        self.assertEqual(p.identity, q.identity)
        with self.assertRaises(ValueError):
            q.foliage(path, 100000, retain=0)

    def test_summary_and_refusal_do_not_construct_detail(self):
        p = plant()
        summary = p.summary()
        self.assertGreater(summary["needles"], 2000)
        self.assertEqual(summary["axes"], 73)
        self.assertFalse(p.wood._axes)
        self.assertFalse(p._groups)
        self.assertIsNone(p.tables(summary["axes"] - 1, summary["needles"]))
        self.assertIsNone(p.tables(summary["axes"], summary["needles"] - 1))
        self.assertIsNone(p.foliage((1, 2), 0))
        self.assertFalse(p.wood._axes)
        self.assertFalse(p._groups)

    def test_global_prefix_matches_independent_subset_and_complete_tables(self):
        p = plant()
        summary = p.summary()
        child_summary = p.summary((3,))
        complete = p.tables(summary["axes"], summary["needles"])
        np.testing.assert_array_equal(
            complete["foliage_ids"], np.arange(summary["needles"])
        )
        np.testing.assert_array_equal(
            complete["foliage_short_ids"], np.arange(summary["groups"])
        )
        self.assertEqual(len(complete["axis_ids"]), summary["axes"])
        self.assertEqual(len(np.unique(complete["foliage_ids"])), summary["needles"])
        expected = p.foliage((3, 2), summary["needles"])
        subset = plant().foliage((3, 2), summary["needles"])
        for key in subset:
            np.testing.assert_array_equal(subset[key], expected[key])
        children = [p.foliage((3,), summary["needles"])]
        children += [p.foliage((3, slot), summary["needles"]) for slot in range(5)]
        self.assertEqual(sum(len(g["ids"]) for g in children), child_summary["needles"])
        ids = np.concatenate([g["ids"] for g in children])
        self.assertEqual(len(np.unique(ids)), child_summary["needles"])
        for key, array in complete.items():
            with self.assertRaises(ValueError, msg=key):
                array.setflags(write=True)

    def test_physical_sizes_and_short_origins_survive_json_replay(self):
        p = plant(2**64 - 1)
        q = NeedledPlant.from_recipe(json.loads(json.dumps(p.recipe())))
        self.assertEqual(p.identity, q.identity)
        self.assertEqual(p.summary(), q.summary())
        summary = p.summary()
        a, b = (
            p.tables(summary["axes"], summary["needles"]),
            q.tables(summary["axes"], summary["needles"]),
        )
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
        for path in ((0, 0), (11, 0)):
            group = p.foliage(path, summary["needles"])
            self.assertTrue(np.all((group["scales"] >= 0.9) & (group["scales"] <= 1.1)))
            tips = (
                group["short_origins"]
                + p.patterns[2].short_length * group["short_frames"][:, :, 1]
            )
            np.testing.assert_array_equal(group["origins"], np.repeat(tips, 5, axis=0))

    def test_bounds_enclose_axis_and_needle_samples(self):
        p = plant()
        radius = p.summary()["root_bound_radius_m"]
        for path in p.wood.paths():
            axis = p.wood.axis(path)
            self.assertLess(np.linalg.norm(axis.origin) + axis.length, radius)
            group = p.foliage(path, 100000)
            if group is not None:
                length = np.array(
                    [
                        p.needles[i].length + p.needles[i].radius
                        for i in group["variants"]
                    ]
                )
                self.assertTrue(
                    np.all(
                        np.linalg.norm(group["origins"], axis=1)
                        + length * group["scales"]
                        < radius
                    )
                )

    def test_invalid_compositions_fail_before_generation(self):
        p = plant()
        for patterns, needles in (
            ((None,) * 3, p.needles),
            (p.patterns[:-1], p.needles),
            (p.patterns, ()),
        ):
            with self.assertRaises(ValueError):
                NeedledPlant(p.wood, patterns, needles)
        with self.assertRaises(ValueError):
            NeedledPlant(
                PlantPlan(0, (AxisLevel(1, 0.1, 0.04, 0.01),), ()),
                (NeedlePattern(),),
                p.needles,
            )
        for key in ("wood", "patterns", "needles"):
            recipe = p.recipe()
            del recipe[key]
            with self.assertRaises(ValueError):
                NeedledPlant.from_recipe(recipe)


if __name__ == "__main__":
    unittest.main()
