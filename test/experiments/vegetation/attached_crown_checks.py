"""Independent scalar hierarchy, physical anchors, demand and stored-product controls."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from attached_crown import AttachedCrown
from attached_crown_build import cluster_recipe
from attached_crown_cost import consume
from crown_shoots import CrownShoots
from outshine_ref.vegetation import Blade, Shoot, TwigClusters
from spatial_group_source import SpatialGroupSource


def source(count=28):
    return SpatialGroupSource(
        CrownShoots(
            np.array(((0, 0, 0), (0, 0, 1), (1, 0, 1)), float),
            np.array((-1, 0, 0)),
            np.arange(count, dtype=np.uint8) % 2,
            682,
            length_m=0.35,
            bend_rad=0.4,
        ),
        (Blade(length=0.14), Blade(length=0.12)),
    )


class AttachedCrownTests(unittest.TestCase):
    def test_scalar_original_twigs_match_full_and_partial_outputs(self):
        s = source(53)
        p = AttachedCrown.build(s)
        old = cluster_recipe(s, p.base, 6)
        for selected in (
            np.ones(2, bool),
            np.array([False, True]),
            np.array([True, False]),
            np.zeros(2, bool),
        ):
            actual = p.evaluate(selected, max_leaves=100000, anchors=True)
            groups = [old.group(int(node), 100000) for node in old.host_ids[selected]]
            for key in ("ids", "variants", "origins", "frames", "nodes", "twig_ids"):
                expected = (
                    np.concatenate([g[key] for g in groups])
                    if groups
                    else np.empty_like(actual[key])
                )
                np.testing.assert_array_equal(actual[key], expected)

    def test_compile_generates_only_bounded_local_prototypes(self):
        original = Shoot.detail
        counts = []

        def bounded(shoot, capacity):
            self.assertLessEqual(capacity, 6)
            counts.append(capacity)
            return original(shoot, capacity)

        with (
            patch.object(Shoot, "detail", bounded),
            patch.object(Blade, "mesh", side_effect=AssertionError),
            patch.object(CrownShoots, "group", side_effect=AssertionError),
        ):
            p = AttachedCrown.build(source(53))
        self.assertLessEqual(len(counts), 96)
        self.assertLessEqual(sum(counts), 53)
        self.assertEqual(int(p.data["counts"].sum()), 53)

    def test_capacity_refusal_precedes_compilation_and_output(self):
        with patch("attached_crown.CompactCrown.build", side_effect=AssertionError):
            self.assertIsNone(AttachedCrown.build(source(), max_twigs=0))
        p = AttachedCrown.build(source())
        with patch("attached_crown_query.np.empty", side_effect=AssertionError):
            self.assertIsNone(p.evaluate(np.ones(2, bool), max_leaves=27))
        for mask, capacity in (([1, 1], 100), ([True], 100), ([True, True], -1)):
            with self.assertRaises(ValueError):
                p.evaluate(mask, max_leaves=capacity)
        for kwargs in ({"leaf_limit": 3}, {"max_twigs": 16385}, {"leaf_limit": True}):
            with self.assertRaises(ValueError):
                AttachedCrown.build(source(), **kwargs)

    def test_fresh_reader_forbids_source_and_random_work(self):
        p = AttachedCrown.build(source())
        expected = p.evaluate(np.ones(2, bool), max_leaves=28, anchors=True)
        with tempfile.TemporaryDirectory(
            prefix="outshine-attached-check-", dir="/tmp"
        ) as tmp:
            path = Path(tmp) / "plan.npz"
            p.save(path)
            with (
                patch.object(AttachedCrown, "build", side_effect=AssertionError),
                patch.object(TwigClusters, "twigs", side_effect=AssertionError),
                patch.object(TwigClusters, "prototype", side_effect=AssertionError),
                patch.object(TwigClusters, "shoot", side_effect=AssertionError),
                patch.object(Shoot, "detail", side_effect=AssertionError),
                patch("numpy.random.Generator", side_effect=AssertionError),
            ):
                q = AttachedCrown.load(path, expected_source=p.base.identity)
                actual = q.evaluate(np.ones(2, bool), max_leaves=28, anchors=True)
            for key in expected:
                np.testing.assert_array_equal(expected[key], actual[key])
            with self.assertRaises(ValueError):
                AttachedCrown.load(path, expected_source="f" * 64)

    def test_geometry_fits_bounds_and_petioles_attach(self):
        s = source(53)
        p = AttachedCrown.build(s)
        rows = p.evaluate(np.ones(2, bool), max_leaves=53, anchors=True)
        owners = np.repeat(np.arange(2), np.diff(p.base.data["offsets"]).astype(int))
        np.testing.assert_allclose(
            rows["origins"] - s.crown.petiole_m * rows["frames"][:, :, 1],
            rows["nodes"],
            atol=3e-15,
        )
        np.testing.assert_allclose(np.linalg.det(rows["frames"]), 1, atol=2e-14)
        for variant, blade in enumerate(s.blades):
            mask = rows["variants"] == variant
            vertices = (
                np.einsum(
                    "nij,vj->nvi", rows["frames"][mask], blade.mesh(32, 4)["vertices"]
                )
                + rows["origins"][mask, None]
            )
            self.assertTrue((vertices >= p.lower[owners[mask], None]).all())
            self.assertTrue((vertices <= p.upper[owners[mask], None]).all())
        for value in p.data.values():
            self.assertFalse(value.flags.writeable)

    def test_zero_inventory_is_valid_without_leaf_prototypes(self):
        with patch.object(Shoot, "detail", side_effect=AssertionError):
            p = AttachedCrown.build(source(0))
        rows = p.evaluate(np.ones(2, bool), max_leaves=0, anchors=True)
        self.assertEqual(len(rows["ids"]), 0)
        self.assertEqual(len(p.data["ids"]), 0)

    def test_none_demand_skips_all_detail_files_and_geometry(self):
        p = AttachedCrown.build(source())
        with tempfile.TemporaryDirectory(
            prefix="outshine-attached-demand-", dir="/tmp"
        ) as tmp:
            folder = Path(tmp)
            np.savez(
                folder / "demand.npz",
                lower=p.lower,
                upper=p.upper,
                offsets=p.base.data["offsets"],
                source=np.asarray(p.base.identity),
            )
            # No compact/full/mmap detail file exists; each reader must still succeed.
            for method in ("compact", "npz", "mmap"):
                result = consume(folder, method, "none")
                self.assertEqual(len(result["ids"]), 0)

    def test_corrupt_saved_parameters_are_rejected(self):
        p = AttachedCrown.build(source())
        with tempfile.TemporaryDirectory(
            prefix="outshine-attached-corrupt-", dir="/tmp"
        ) as tmp:
            path = Path(tmp) / "plan.npz"
            p.save(path)
            with np.load(path, allow_pickle=False) as z:
                arrays = {k: z[k] for k in z.files}
            arrays["twig__origins"][0, 0] += 0.01
            np.savez(path, **arrays)
            with self.assertRaises(ValueError):
                AttachedCrown.load(path, expected_source=p.base.identity)


if __name__ == "__main__":
    unittest.main()
