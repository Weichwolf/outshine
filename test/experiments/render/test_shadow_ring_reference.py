"""Independent pixel/PCF enumerations and real-coefficient arithmetic counterexamples."""

import gzip
import json
import unittest
from fractions import Fraction

import numpy as np
from shadow_rebase_arithmetic import packed_view, probe
from shadow_ring_reference import (
    Tile,
    exposed_rectangles,
    logical,
    pcf,
    pcf_scalar,
    raster,
    update,
)
from shadow_translation_cost import FIXTURE

IDENTITY = np.eye(4).flatten(order="F").tolist()
TRIANGLES = np.array(
    [
        [[-1.6, -1.3, 0.25], [1.7, -1.2, 0.25], [-1.4, 1.8, 0.25]],
        [[-0.4, -0.3, 0.7], [0.9, -0.2, 0.7], [0.1, 0.8, 0.7]],
    ]
)


def translated(dx, dy, side):
    result = IDENTITY.copy()
    result[12], result[13] = dx * 2 / side, -dy * 2 / side
    return result


class ShadowRingReference(unittest.TestCase):
    def test_disjoint_strips_against_pixel_enumeration(self):
        side = 8
        for dx in range(-9, 10):
            for dy in range(-9, 10):
                seen = []
                for x0, y0, x1, y1 in exposed_rectangles(side, dx, dy):
                    seen.extend((x, y) for y in range(y0, y1) for x in range(x0, x1))
                expected = {
                    (x, y)
                    for y in range(side)
                    for x in range(side)
                    if not (0 <= x - dx < side and 0 <= y - dy < side)
                }
                self.assertEqual(set(seen), expected)
                self.assertEqual(len(seen), len(expected))

    def test_raster_pixel_centres_depth_and_offscreen_caster(self):
        depth = raster(TRIANGLES[:1], IDENTITY, 16)
        for y in range(16):
            for x in range(16):
                p = np.array([2 * (x + 0.5) / 16 - 1, 1 - 2 * (y + 0.5) / 16])
                a, b, c = TRIANGLES[0, :, :2]
                u, v = np.linalg.solve(np.stack([b - a, c - a], axis=1), p - a)
                self.assertEqual(
                    depth[y, x], 0.25 if u >= 0 and v >= 0 and u + v <= 1 else 0
                )
        self.assertGreater(np.count_nonzero(depth), 0)

    def test_full_copy_ring_wrap_and_all_signs(self):
        for ring in (False, True):
            tile = None
            motions = [(0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (3, -2), (-2, 3)]
            motions += [(i, -i) for i in range(4, 40)] + [(80, 80), (-80, -80)]
            for dx, dy in motions:
                matrix = translated(dx, dy, 16)
                tile, _ = update(tile, TRIANGLES, matrix, 16, ring=ring)
                np.testing.assert_array_equal(
                    logical(tile), raster(TRIANGLES, matrix, 16)
                )

    def test_retry_reset_resolution_depth_light_and_dynamic_removal(self):
        rejected_first, _ = update(None, TRIANGLES, IDENTITY, 16, accepted=False)
        self.assertIsNone(rejected_first)
        old, _ = update(None, TRIANGLES, IDENTITY, 16)
        before = old.data.copy()
        new = translated(1, -1, 16)
        rejected, _ = update(old, TRIANGLES, new, 16, accepted=False)
        self.assertIs(rejected, old)
        np.testing.assert_array_equal(old.data, before)
        self.assertEqual(old.matrix, IDENTITY)
        accepted, _ = update(rejected, TRIANGLES, new, 16)
        np.testing.assert_array_equal(logical(accepted), raster(TRIANGLES, new, 16))
        for index in (0, 1, 2, 10, 12, 14):
            changed = new.copy()
            changed[index] += 0.03125
            reset, costs = update(accepted, TRIANGLES, changed, 16)
            self.assertEqual(costs["new_texels"], 256)
            np.testing.assert_array_equal(
                logical(reset), raster(TRIANGLES, changed, 16)
            )
        removed, costs = update(accepted, TRIANGLES[:1], new, 16, content=1)
        self.assertEqual(costs["new_texels"], 256)
        np.testing.assert_array_equal(logical(removed), raster(TRIANGLES[:1], new, 16))
        resized, costs = update(accepted, TRIANGLES, new, 8)
        self.assertEqual(costs["new_texels"], 64)
        np.testing.assert_array_equal(logical(resized), raster(TRIANGLES, new, 8))

    def test_sloped_depth_overlap_and_depth_clipping(self):
        triangle = np.array([[[-1, -1, 0.2], [3, -1, 0.6], [-1, 3, 1.0]]])
        duplicate = triangle.copy()
        duplicate[:, :, 2] -= 0.1
        depth = raster(np.concatenate([triangle, duplicate]), IDENTITY, 8)
        y, x = np.mgrid[:8, :8]
        expected = 0.2 + 0.1 * (2 * (x + 0.5) / 8) + 0.2 * (2 - 2 * (y + 0.5) / 8)
        np.testing.assert_array_equal(depth, expected.astype(np.float32))
        triangle[:, :, 2] += 1
        np.testing.assert_array_equal(raster(triangle, IDENTITY, 8), np.zeros((8, 8)))

    def test_pcf_all_origins_clamps_and_receiver_slopes(self):
        side = 8
        depth = (np.arange(side * side).reshape(side, side) % 11 / 10).astype(
            np.float32
        )
        grid = np.array([-0.2, 0, 0.0625, 0.1, 0.5, 0.91, 0.9375, 1, 1.2])
        uv = np.stack(np.meshgrid(grid, grid), axis=-1).reshape(-1, 2)
        slope = np.tile([0.13, -0.17], (len(uv), 1))
        receiver = np.full(len(uv), 0.47)
        expected = np.array(
            [
                pcf_scalar(depth, u, r, s)
                for u, r, s in zip(uv, receiver, slope, strict=True)
            ]
        )
        wrong = 0
        for x in range(side):
            for y in range(side):
                physical = np.roll(depth, (y, x), axis=(0, 1))
                tile = Tile(IDENTITY, 0, physical, (x, y))
                np.testing.assert_allclose(
                    pcf(tile, uv, receiver, slope), expected, rtol=0, atol=1e-15
                )
                wrong += np.count_nonzero(
                    pcf(tile, uv, receiver, slope, broken_physical_offsets=True)
                    != expected
                )
        self.assertGreater(wrong, 0)

    def test_real_host_rebase_counterexample_against_rational_float32_steps(self):
        fixture = json.loads(gzip.decompress(FIXTURE.read_bytes()))
        report = probe(fixture)
        self.assertEqual(len(report["witnesses"]), 8)
        anchor = fixture["frames"][0]["centre"]

        def rounded(value):
            nominal = np.float32(float(value))
            choices = [
                np.nextafter(nominal, np.float32(-np.inf)),
                nominal,
                np.nextafter(nominal, np.float32(np.inf)),
            ]
            return min(
                choices,
                key=lambda x: (
                    abs(Fraction(float(x)) - value),
                    int(x.view(np.uint32)) % 2,
                ),
            )

        for witness in report["witnesses"]:
            self.assertNotEqual(witness["step_before"], witness["step_after"])
            self.assertTrue(
                fixture["observed"][witness["frame"] - 1]["mask"]
                & (1 << witness["region"])
            )
            for index, output in (
                (witness["previous_update_frame"], "before"),
                (witness["frame"], "after"),
            ):
                matrix, shift = packed_view(
                    fixture["frames"][index], witness["region"], anchor
                )
                values = [
                    rounded(Fraction(x) + Fraction(float(s)))
                    for x, s in zip(witness["point"], shift, strict=True)
                ]
                exact = np.float32(0)
                for axis in range(3):
                    product = Fraction(float(values[axis])) * Fraction(
                        float(matrix[2, axis])
                    )
                    if not witness["fused"]:
                        product = Fraction(float(rounded(product)))
                    exact = rounded(Fraction(float(exact)) + product)
                exact = rounded(Fraction(float(exact)) + Fraction(float(matrix[2, 3])))
                self.assertEqual(float(exact), witness[output][2])


if __name__ == "__main__":
    unittest.main()
