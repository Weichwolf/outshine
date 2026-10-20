"""Independent perspective fields, trim edges, disocclusion and conservative bins."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from compare_projective_field import factorized_control
from packed_normals import pack
from projective_field_bake import cook, encode
from projective_field_bins import build, pairs
from projective_field_query import load_product, prepare, query


def source_quad(depth=5, half=1, slope=(0.2, 0.1), source_id=17, code=2):
    xy = np.array([[-half, -half], [-half, half], [half, -half], [half, half]])
    xyz = np.column_stack((xy, depth + xy @ slope))
    normal = np.array([*slope, -1.0])
    normal /= np.linalg.norm(normal)
    return {
        "positions": xyz,
        "indices": np.array([[0, 1, 2], [1, 3, 2]]),
        "normal": pack(np.tile(normal, (4, 1))),
        "uv": np.column_stack(
            (256 * code + 3 * (xy[:, 0] + 1), 2 * (xy[:, 1] + 1))
        ).astype(np.float32),
        "metres": np.column_stack((10 * (xy[:, 0] + 1), 7 * (xy[:, 1] + 1))).astype(
            np.float32
        ),
        "colour": np.column_stack(
            (
                0.3 + 0.1 * xy[:, 0],
                0.4 + 0.1 * xy[:, 1],
                np.full(4, 0.2),
                0.6 + 0.05 * xy[:, 0],
            )
        ),
        "binding": np.array(
            [[0, source_id, -1, -1, 0, 11], [0, source_id, -1, -1, 0, 12]], np.int32
        ),
    }


def joined(sources):
    result = {key: [] for key in sources[0]}
    offset = 0
    for source in sources:
        for key, value in source.items():
            result[key].append(value + offset if key == "indices" else value)
        offset += len(source["positions"])
    return {key: np.concatenate(value) for key, value in result.items()}


class ProjectiveFieldTest(unittest.TestCase):
    def setUp(self):
        self.profile = {"width": 1280, "height": 720, "focal_px": 700}
        self.box = np.array([[-0.4, -0.2, 0], [0.4, 0.2, 0]])

    def product(self, source, dtype=np.float64):
        a, _ = encode(source, [0, 1, 0], self.profile, dtype=dtype)
        return prepare(a, {"coordinate_bounds": [[-1e7] * 3, [1e7] * 3]})

    def test_sloped_perspective_affine_fields_alpha_and_lineage(self):
        for dtype, tolerance in ((np.float64, 1e-10), (np.float32, 1e-5)):
            source = source_quad()
            p = self.product(source, dtype)
            o, d = np.array([[0.1, -0.1, 0]]), np.array([[0.04, 0.02, 1]])
            depth, fields, owner, tag = query(p, o, d)
            expected = (5 + 0.2 * o[0, 0] + 0.1 * o[0, 1]) / (
                1 - 0.2 * d[0, 0] - 0.1 * d[0, 1]
            )
            np.testing.assert_allclose(depth, [expected], atol=tolerance, rtol=0)
            x, y, _ = (o + expected * d)[0]
            np.testing.assert_allclose(
                fields["coordinates"],
                [[3 * (x + 1), 2 * (y + 1)]],
                atol=tolerance,
                rtol=0,
            )
            np.testing.assert_allclose(
                fields["metres"], [[10 * (x + 1), 7 * (y + 1)]], atol=tolerance, rtol=0
            )
            np.testing.assert_allclose(
                fields["colour"],
                [[0.3 + 0.1 * x, 0.4 + 0.1 * y, 0.2, 0.6 + 0.05 * x]],
                atol=tolerance,
                rtol=0,
            )
            np.testing.assert_array_equal(fields["code"], [2])
            np.testing.assert_array_equal(owner, [17])
            self.assertIn(int(tag[0]), (0, 1))
            np.testing.assert_array_equal(p["arrays"]["native_face_tag"], [11, 12])

    def test_trim_keeps_exterior_transparent(self):
        p = self.product(source_quad(slope=(0, 0)))
        depth, _, owner, _ = query(
            p, np.zeros((2, 3)), np.array([[0.1, 0, 1], [0.21, 0, 1]])
        )
        self.assertTrue(np.isfinite(depth[0]))
        self.assertTrue(np.isinf(depth[1]))
        np.testing.assert_array_equal(owner, [17, -1])

    def test_hidden_layers_and_actual_disocclusion(self):
        sources = [
            source_quad(
                depth=3 + i,
                half=0.2 if i == 0 else 1,
                slope=(0, 0),
                source_id=20 + i,
                code=2 + i,
            )
            for i in range(7)
        ]
        p = self.product(joined(sources))
        self.assertEqual(len(p["arrays"]["original_face"]), 14)
        depth, fields, owner, _ = query(
            p, np.array([[0, 0, 0], [0.4, 0, 0]]), np.array([[0, 0, 1], [0, 0, 1]])
        )
        np.testing.assert_allclose(depth, [3, 4])
        np.testing.assert_array_equal(owner, [20, 21])
        np.testing.assert_array_equal(fields["code"], [2, 3])

    def test_near_clip_and_backward_rays(self):
        source = joined(
            [
                source_quad(depth=5, slope=(0, 0)),
                source_quad(depth=20, slope=(0, 0), source_id=22),
            ]
        )
        p = self.product(source)
        depth, _, owner, _ = query(
            p, np.zeros((2, 3)), np.array([[0, 0, 100], [0, 0, -1]])
        )
        np.testing.assert_allclose(depth[:1], [0.2])
        self.assertTrue(np.isinf(depth[1]))
        np.testing.assert_array_equal(owner, [22, -1])

    def test_stored_identity_and_domain_before_payload(self):
        with tempfile.TemporaryDirectory(prefix="outshine-projective-field-") as tmp:
            path = Path(tmp) / "field"
            cook(
                source_quad(),
                [0, 1, 0],
                self.profile,
                path,
                {"input": "plane"},
            )
            p = load_product(path, [[0, 0, 0]], {"input": "plane"})
            depth, _, _, _ = query(p, np.zeros((1, 3)), np.array([[0, 0, 1]]))
            np.testing.assert_allclose(depth, [5], atol=1e-5)
            (path / "field.npz").unlink()
            with self.assertRaisesRegex(ValueError, "outside supported domain"):
                load_product(path, [[1e8, 0, 0]], {"input": "plane"})
            with self.assertRaisesRegex(ValueError, "identity/format"):
                load_product(path, [[0, 0, 0]], {"input": "other"})

    def test_same_full_surface_field_queries_from_opposite_side(self):
        front = source_quad(slope=(0, 0))
        back = source_quad(slope=(0, 0), source_id=44, code=3)
        back["indices"] = back["indices"][:, ::-1]
        back["normal"] = pack(np.tile([0, 0, 1.0], (4, 1)))
        p = self.product(joined([front, back]))
        depth, fields, owners, _ = query(
            p,
            np.array([[0, 0, 0], [0, 0, 10], [2, 0, 5]]),
            np.array([[0, 0, 1], [0, 0, -1], [0, 1, 0]]),
        )
        np.testing.assert_allclose(depth[:2], [5, 5], atol=1e-10)
        self.assertTrue(np.isinf(depth[2]))
        np.testing.assert_array_equal(owners, [17, 44, -1])
        np.testing.assert_array_equal(fields["code"], [2, 3, 0])

    def test_bin_boundary_segments_and_preallocation_budget(self):
        triangles = np.array(
            [[[0, 0], [1, 0], [0, 1]], [[1, 1], [2, 1], [1, 2]]], float
        )
        a = build(triangles)
        rays, faces, _ = pairs(
            a, np.array([[0, 0], [1, -1]]), np.array([[2, 2], [1, 3]]), np.ones(2, bool)
        )
        for ray in (0, 1):
            np.testing.assert_array_equal(faces[rays == ray], [0, 1])
        with self.assertRaisesRegex(ValueError, "finer spatial node"):
            build(triangles * 1e6)

    def test_factorized_control_keeps_nonwhite_alpha_and_native_bits(self):
        source = source_quad()
        for key in ("positions", "uv", "metres", "colour"):
            source[key] = source[key].astype(np.float32)
        p = self.product(source)
        arrays, report = factorized_control(source, p["arrays"])
        restored = arrays["attributes"].view(np.float32)[arrays["attribute_indices"]]
        np.testing.assert_array_equal(
            restored[..., -1], source["colour"][source["indices"], 3]
        )
        self.assertEqual(
            report["total_array_bytes"], sum(a.nbytes for a in arrays.values())
        )


if __name__ == "__main__":
    unittest.main()
