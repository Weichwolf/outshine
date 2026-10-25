"""A ready parent read must not materialize its source-lineage arrays."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_mass_appearance import group_materials
from building_mass_materials import policy
from building_tree_load import load_cut, read_tree, render_arrays
from building_tree_products import build_cut
from test_building_source_tree import tree_and_frame


class BuildingTreeLoad(unittest.TestCase):
    def test_ready_parent_skips_source_lineage_and_original_map(self):
        tree, frame, masses = tree_and_frame()
        frame["tree_sha256"] = np.arange(32, dtype=np.uint8)
        tree.update(frame, unbounded_sources=np.empty(0, np.uint32))
        product, _ = build_cut(tree, [0], masses, frame)
        at = policy()
        measure = np.zeros((2, 4 + len(at["palette"])))
        measure[:, :3] = [0.3, 0.4, 0.5]
        measure[:, 3] = 1
        measure[:, -1] = 1
        material = group_materials({1: measure, 2: measure}, product, at)
        with tempfile.TemporaryDirectory(prefix="outshine-tree-load-") as directory:
            root = Path(directory)
            np.savez(root / "tree.npz", **tree)
            np.savez(
                root / "node.npz",
                **{"g__" + k: v for k, v in product.items()},
                **{"m__" + k: v for k, v in material.items()},
            )
            with np.load(root / "node.npz", allow_pickle=False) as archive:
                cls = type(archive)
            original = cls.__getitem__
            calls = []

            def getitem(archive, key):
                self.assertNotIn(
                    key,
                    {
                        "g__group_sources",
                        "g__group_offsets",
                        "g__selected_sources",
                        "source_order",
                        "node_sources",
                    },
                )
                calls.append(key)
                return original(archive, key)

            query = {"eye": [1, -10000, 2], "focal_px": 900, "nodes": [0]}
            with patch.object(cls, "__getitem__", getitem):
                runtime_tree = read_tree(root, lineage=False)
                geometry, appearance, work = load_cut(
                    root, {"node_files": {"0": "node.npz"}}, runtime_tree, query, at
                )
            self.assertGreater(len(geometry["indices"]), 0)
            self.assertTrue(appearance["supported"].all())
            self.assertNotIn("source_order", runtime_tree)
            self.assertEqual(work["parent_lineage_arrays_read"], 0)
            self.assertEqual(
                work["source_map_array_bytes_read_for_original_fallback"], 0
            )
            ready = {"node.npz": render_arrays(root / "node.npz")}
            with patch(
                "building_tree_load.render_arrays",
                side_effect=AssertionError(
                    "unchanged demand must not reread parent arrays"
                ),
            ):
                reused, _, cost = load_cut(
                    root,
                    {"node_files": {"0": "node.npz"}},
                    runtime_tree,
                    query,
                    at,
                    ready_arrays=ready,
                )
            np.testing.assert_array_equal(reused["indices"], geometry["indices"])
            self.assertEqual(cost["parent_render_array_bytes_read"], 0)
            self.assertEqual(cost["reused_parent_files"], ["node.npz"])
            wrong = runtime_tree | {"tree_sha256": runtime_tree["tree_sha256"] ^ 1}
            with self.assertRaisesRegex(ValueError, "different source hierarchy"):
                load_cut(
                    root,
                    {"node_files": {"0": "node.npz"}},
                    wrong,
                    query,
                    at,
                    ready_arrays=ready,
                )


if __name__ == "__main__":
    unittest.main()
