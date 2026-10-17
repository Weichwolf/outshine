"""Same prototype raster, visible-only moments, unit normals and bounded capture."""

import unittest
from unittest.mock import patch

import numpy as np
from conifer_window_catalog import template_image
from normal_lobes import NormalMeasure, evaluate
from prototype_normal_bank import encode


def square():
    return {
        "vertices": np.array(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)), float),
        "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32),
        "normals": np.broadcast_to((0.0, 0.0, 1.0), (2, 3, 3)).copy(),
        "colour": np.full((2, 3, 3), 0.2),
        "leaf": np.ones(2, bool),
    }


def projection():
    return np.eye(3), np.array((-1.0, -1.0)), np.array((1.0, 1.0)), 0


class PrototypeNormalTests(unittest.TestCase):
    def test_direct_vector_bank_encoding_preserves_a_flat_source(self):
        image, _ = template_image(
            square(), [0, 0, 1], 32, 8, projection_data=projection()
        )
        bank = {"images": image[None]}
        bank.update(
            {
                key: np.asarray(value)[None]
                for key, value in zip(
                    ("frames", "lower", "upper", "offsets"), projection(), strict=True
                )
            }
        )
        vectors, old, facts = encode(square(), bank, 0)
        np.testing.assert_array_equal(vectors.sum(axis=2), old)
        np.testing.assert_array_equal(old, np.broadcast_to([0, 0, 1], (32, 32, 3)))
        self.assertEqual(vectors.dtype, np.dtype("float32"))
        self.assertEqual(facts["float32_vector_error_bound_max"], 0)

    def test_requested_partitions_avoid_unneeded_fields_without_changing_values(self):
        selected = NormalMeasure(4, np.eye(3), partitions=(4,))
        complete = NormalMeasure(4, np.eye(3))
        for measure in (selected, complete):
            template_image(
                square(),
                [0, 0, 1],
                4,
                2,
                projection_data=projection(),
                normal_measure=measure,
            )
        self.assertEqual(set(selected.moments), {4})
        np.testing.assert_array_equal(selected.moments[4], complete.moments[4])
        np.testing.assert_array_equal(selected.exact, complete.exact)
        for partitions in ((), (4, 4), (True,), (5,)):
            with self.assertRaisesRegex(ValueError, "partitions"):
                NormalMeasure(4, np.eye(3), partitions=partitions)

    def test_existing_image_is_exact_and_flat_response_has_no_side_or_back_leak(self):
        source = square()
        old, _ = template_image(source, [0, 0, 1], 32, 2, projection_data=projection())
        measure = NormalMeasure(32, np.r_[np.eye(3), -np.eye(3)])
        actual, _ = template_image(
            source,
            [0, 0, 1],
            32,
            2,
            projection_data=projection(),
            normal_measure=measure,
        )
        np.testing.assert_array_equal(actual, old)
        expected = np.zeros((6, 1024))
        expected[2] = 1
        np.testing.assert_array_equal(measure.exact, expected)
        for moments in measure.moments.values():
            np.testing.assert_array_equal(
                evaluate(moments[..., 1:], measure.directions), expected
            )
            np.testing.assert_array_equal(
                moments[..., 0].sum(axis=1), actual[..., 3].ravel()
            )

    def test_occluded_back_layer_is_not_counted(self):
        source = square()
        source["vertices"] = np.r_[source["vertices"], source["vertices"] - [0, 0, 1]]
        source["faces"] = np.r_[source["faces"], source["faces"] + 4]
        source["normals"] = np.r_[source["normals"], -source["normals"]]
        source["colour"] = np.r_[source["colour"], source["colour"]]
        source["leaf"] = np.ones(4, bool)
        measure = NormalMeasure(4, [[0, 0, 1], [0, 0, -1]])
        template_image(
            source,
            [0, 0, 1],
            4,
            4,
            projection_data=projection(),
            normal_measure=measure,
        )
        np.testing.assert_array_equal(
            measure.exact, np.r_[np.ones((1, 16)), np.zeros((1, 16))]
        )
        self.assertEqual(measure.sample_count, 256)

    def test_float32_interpolation_is_renormalized_only_for_capture(self):
        source = square()
        source["normals"][:] = np.array([0.3, 0.4, np.sqrt(0.75)])
        old, _ = template_image(source, [0, 0, 1], 4, 2, projection_data=projection())
        measure = NormalMeasure(4, [[1.0, 0, 0]])
        image, _ = template_image(
            source,
            [0, 0, 1],
            4,
            2,
            projection_data=projection(),
            normal_measure=measure,
        )
        np.testing.assert_array_equal(image, old)
        np.testing.assert_allclose(measure.exact, 0.3, atol=3e-8, rtol=0)

    def test_mismatched_capture_refuses_before_projection_or_raster(self):
        with (
            patch(
                "conifer_window_catalog.rasterize", side_effect=AssertionError("raster")
            ),
            self.assertRaisesRegex(ValueError, "same native"),
        ):
            template_image(
                {}, [0, 0, 1], 32, 8, normal_measure=NormalMeasure(16, np.eye(3))
            )

    def test_large_covered_raster_is_packet_bounded(self):
        measure = NormalMeasure(32, np.eye(3))
        original = measure.add
        sizes = []

        def add(slots, mass, normals):
            sizes.append(len(slots))
            original(slots, mass, normals)

        measure.add = add
        template_image(
            square(),
            [0, 0, 1],
            32,
            8,
            projection_data=projection(),
            normal_measure=measure,
        )
        self.assertEqual(sizes, [8192] * 8)
        self.assertEqual(measure.sample_count, 65536)
        np.testing.assert_array_equal(
            measure.moments[4][..., 0].sum(axis=1), np.ones(1024)
        )


if __name__ == "__main__":
    unittest.main()
