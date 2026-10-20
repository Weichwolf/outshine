"""Standard-library ABI, categorical seams and real no-change preflight controls."""

import unittest

import numpy as np
from attributed_prisms import native_inputs
from constrained_mesh_simplification import (
    categorical_locks,
    coupled_vertices,
    installed_library,
    simplify,
)
from packed_normals import pack


def square(source_id=17, code=0, shift=0):
    p = np.array(
        [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0], [0.5, 0.5, 0]], np.float32
    )
    p[:, 0] += shift
    indices = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]], np.uint32)
    uv = p[:, :2].copy()
    uv[:, 0] += 256 * code
    return {
        "positions": p,
        "indices": indices,
        "uv": uv,
        "normal": np.full(5, pack([[0, 0, 1]])[0], np.uint32),
        "metres": p[:, :2].copy(),
        "colour": np.ones((5, 4), np.float32),
        "binding": np.array([[0, source_id, 0, -1, 0, i] for i in range(4)], np.int32),
    }


def joined(first, second):
    fields = {
        k: np.concatenate([first[k], second[k]])
        for k in ("positions", "uv", "normal", "metres", "colour", "binding")
    }
    fields["indices"] = np.concatenate(
        [first["indices"], second["indices"] + len(first["positions"])]
    )
    return fields


class ConstrainedSimplification(unittest.TestCase):
    library = installed_library()

    def test_actual_strict_and_permissive_abi_reduce_affine_planar_grid(self):
        for kwargs in ({}, {"library": self.library, "permissive": True}):
            source = square()
            output, report = simplify(source, **kwargs)
            self.assertEqual(len(output["indices"]), 2)
            self.assertEqual(report["output_lineages"], 1)
            np.testing.assert_array_equal(output["binding"][:, 1], [17, 17])
            points = output["positions"][output["indices"]]
            area = (
                np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])[
                    :, 2
                ].sum()
                / 2
            )
            self.assertAlmostEqual(area, 1)

    def test_nonaffine_uv_discontinuity_cannot_be_silently_discarded(self):
        source = square()
        source["uv"][4, 0] = 0.8
        output, _ = simplify(
            source, library=self.library, permissive=True, absolute_error_m=1e-5
        )
        self.assertEqual(len(output["indices"]), 4)
        self.assertTrue(np.any(output["uv"][:, 0] == np.float32(0.8)))

    def test_touching_sources_and_materials_remain_discrete(self):
        for first, second in (
            (square(), square(18, shift=1)),
            (square(), square(code=1, shift=1)),
        ):
            source = joined(first, second)
            prepared = coupled_vertices(source)
            self.assertEqual(int(np.count_nonzero(categorical_locks(prepared))), 4)
            output, _ = simplify(source, library=self.library, permissive=True)
            self.assertEqual(len(output["indices"]), 4)
            before, after = native_inputs(source), native_inputs(output)
            np.testing.assert_array_equal(
                np.unique(before["flat_codes"]), np.unique(after["flat_codes"])
            )
            np.testing.assert_array_equal(
                np.unique(source["binding"][:, 1]), np.unique(output["binding"][:, 1])
            )

    def test_signed_zero_cannot_hide_a_categorical_seam(self):
        source = joined(square(), square(18))
        source["positions"][5, 0] = -0.0
        prepared = coupled_vertices(source)
        self.assertEqual(int(np.count_nonzero(categorical_locks(prepared))), 10)

    def test_input_contract_precedes_native_call(self):
        source = square()
        source["indices"][0, 0] = 99
        with self.assertRaisesRegex(ValueError, "bounded valid native"):
            simplify(source)
        with self.assertRaises(ValueError):
            simplify(square(), absolute_error_m=float("nan"))


if __name__ == "__main__":
    unittest.main()
