"""Independent transformed corners, organ ownership and preflight boundaries."""

import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Blade
from PIL import Image
from scipy.spatial.transform import Rotation
from stand_leaf_pack import pack_leaves
from stand_leaf_product import build, ready


def fixture():
    meshes = []
    for width in (0.025, 0.04):
        m = Blade(length=0.1, width=width).mesh(3, 1)
        m["corner_uv"] = np.c_[(m["parameters"][:, 1] + 1) / 2, m["parameters"][:, 0]][
            m["faces"].ravel()
        ]
        m["surface"] = np.full(len(m["faces"]), 6, np.uint8)
        meshes.append(m)
    rows = {
        "leaf_ids": np.array((713, 18, 90, 300), np.uint64),
        "leaf_variants": np.array((1, 0, 1, 0), np.uint8),
        "leaf_origins": np.array(((0, 0, 1), (2, 0, 1), (0, 2, 1), (2, 2, 1)), float),
        "leaf_frames": Rotation.from_rotvec(
            ((0, 0, 0), (0.2, 0.4, 0), (0, 0.7, 0.3), (0.8, 0.2, -0.3))
        ).as_matrix(),
    }
    return rows, meshes


def fixture_files(folder):
    rows, meshes = fixture()
    folder.mkdir()
    np.savez_compressed(folder / "tables.npz", **rows)
    np.savez_compressed(folder / "wood.npz", **meshes[0])
    for i in range(8):
        np.savez_compressed(folder / f"blade-{i}.npz", **meshes[i % 2])
    (folder / "entry.json").write_text(
        json.dumps(
            {
                "plan_identity": "a" * 64,
                "format": "fixture",
                "tables": "tables.npz",
                "products": ["wood.npz", *[f"blade-{i}.npz" for i in range(8)]],
                "instanced_triangles": sum(
                    len(meshes[int(v)]["faces"]) for v in rows["leaf_variants"]
                )
                + len(meshes[0]["faces"]),
            }
        )
    )
    for name in ("fern-colour.png", "fern-height.png"):
        Image.new("RGBA", (2, 2), (20, 50, 10, 255)).save(folder / name)
    (folder / "fern-material.json").write_text(
        json.dumps(
            {
                "colour": "fern-colour.png",
                "height": "fern-height.png",
                "height_scale_m": 0.001,
            }
        )
    )


class StandPackChecks(unittest.TestCase):
    def test_independent_scalar_points_normals_uvs_and_owner_ranges(self):
        rows, meshes = fixture()
        result, runs, report = pack_leaves(rows, meshes, packet=1)
        seen = []
        for i, identity in enumerate(runs["ids"]):
            row = int(np.flatnonzero(rows["leaf_ids"] == identity)[0])
            v = int(rows["leaf_variants"][row])
            mesh = meshes[v]
            start, n = int(runs["first_vertex"][i]), int(runs["vertices"][i])
            first, count = int(runs["first_face"][i]), int(runs["faces"][i])
            expected = np.array(
                [
                    rows["leaf_frames"][row] @ point + rows["leaf_origins"][row]
                    for point in mesh["vertices"]
                ]
            )
            normals = np.array(
                [rows["leaf_frames"][row] @ normal for normal in mesh["normals"]]
            )
            np.testing.assert_allclose(
                result["vertices"][start : start + n], expected, atol=1e-6, rtol=0
            )
            np.testing.assert_allclose(
                result["normals"][first * 3 : (first + count) * 3],
                normals,
                atol=1e-6,
                rtol=0,
            )
            np.testing.assert_array_equal(
                result["faces"][first : first + count], mesh["faces"] + start
            )
            np.testing.assert_array_equal(
                result["corner_uv"][first * 3 : (first + count) * 3],
                mesh["corner_uv"].astype(np.float32),
            )
            seen.extend(range(first, first + count))
        self.assertEqual(seen, list(range(len(result["faces"]))))
        np.testing.assert_array_equal(np.sort(runs["ids"]), np.sort(rows["leaf_ids"]))
        self.assertLessEqual(report["maximum_position_error_m"], 1e-6)

    def test_packet_size_keeps_the_identical_float32_product(self):
        rows, meshes = fixture()
        first = pack_leaves(rows, meshes, packet=1)
        for packet in (2, 1024):
            actual = pack_leaves(rows, meshes, packet=packet)
            for a, b in zip(first[:2], actual[:2]):
                for key in a:
                    np.testing.assert_array_equal(a[key], b[key])

    def test_small_buffer_or_bad_quantization_is_refused_before_expansion(self):
        rows, meshes = fixture()
        with patch(
            "stand_leaf_pack.Assembly.add",
            side_effect=AssertionError("expanded before refusal"),
        ):
            with self.assertRaises(ValueError):
                pack_leaves(rows, meshes, max_bytes=1)
            other = rows | {"leaf_origins": rows["leaf_origins"] + 100000}
            with self.assertRaises(ValueError):
                pack_leaves(other, meshes)
            with self.assertRaises(ValueError):
                pack_leaves(rows, meshes, packet=5000)

    def test_invalid_frames_variants_or_ids_are_rejected(self):
        rows, meshes = fixture()
        for change in (
            {"leaf_ids": np.ones(4, np.uint64)},
            {"leaf_variants": np.full(4, 2, np.uint8)},
            {"leaf_frames": np.zeros((4, 3, 3))},
        ):
            with self.assertRaises(ValueError):
                pack_leaves(rows | change, meshes)

    def test_empty_demand_keeps_a_complete_empty_product(self):
        rows, meshes = fixture()
        empty = {k: v[:0] for k, v in rows.items()}
        result, runs, report = pack_leaves(empty, meshes)
        self.assertEqual(result["faces"].shape, (0, 3))
        self.assertEqual(len(runs["ids"]), 0)
        self.assertEqual(report["faces"], 0)

    def test_finished_hit_is_independent_of_source_and_rejects_corruption(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.vegetation-packed-"
        ) as temp:
            root = Path(temp)
            source = root / "source"
            product = root / "ready"
            fixture_files(source)
            build(source, product)
            metadata = json.loads((product / "entry.json").read_text())
            shutil.rmtree(source)
            with patch(
                "stand_leaf_product.pack_leaves",
                side_effect=AssertionError("regeneration on hit"),
            ):
                actual, data = ready(product, metadata["source_product_sha256"])
            self.assertEqual(actual["identity"], metadata["identity"])
            self.assertIn("leaf-mesh.npz", data)
            with self.assertRaises(ValueError):
                ready(product, "0" * 64)
            (product / "entry.json").write_text(
                json.dumps(metadata | {"plan_identity": "wrong"})
            )
            with self.assertRaises(ValueError):
                ready(product)
            (product / "entry.json").write_text(json.dumps(metadata))
            with (product / "leaf-mesh.npz").open("ab") as f:
                f.write(b"changed")
            with self.assertRaises(ValueError):
                ready(product)

    def test_changed_source_geometry_cannot_hit_the_same_plan_named_product(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.vegetation-packed-source-"
        ) as temp:
            root = Path(temp)
            source = root / "source"
            fixture_files(source)
            build(source, root / "first")
            with np.load(source / "blade-0.npz") as d:
                mesh = {k: d[k] for k in d.files}
            mesh["vertices"] *= 1.05
            np.savez_compressed(source / "blade-0.npz", **mesh)
            build(source, root / "second")
            a, _ = ready(root / "first")
            b, _ = ready(root / "second")
            self.assertEqual(a["plan_identity"], b["plan_identity"])
            self.assertNotEqual(a["source_product_sha256"], b["source_product_sha256"])
            with self.assertRaises(ValueError):
                ready(root / "first", b["source_product_sha256"])


if __name__ == "__main__":
    unittest.main()
