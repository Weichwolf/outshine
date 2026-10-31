import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conforming_body_contact import grid
from outshine_ref.contracts import LocalFrame
from shore_bank_strip import build
from shore_contact_parts import cell_groups, prepare, select
from shore_contact_parts_study import pack, read, sequence
from shore_terrain_contact_study import reconcile


class CanonicalContactParts(unittest.TestCase):
    def data(self):
        xy, faces = grid(np.arange(9) / 2, np.arange(-2, 7) / 2)
        vertices = np.c_[xy, 12 + xy[:, 0] / 8 + xy[:, 1] / 4]
        body = build(
            [[-1, 0, 10], [5, 0, 10.5]],
            [15, 15.5],
            [[-1, 1, 16], [5, 1, 16.5]],
            bed_depth_m=2,
        )
        import shapely

        footprint = shapely.coverage_union_all(
            shapely.polygons(body["vertices"][body["roof_faces"], :2])
        )
        products, _ = reconcile(body, vertices, faces, footprint)
        return products, faces

    def test_every_selected_facet_and_source_is_the_canonical_plan(self):
        products, original = self.data()
        field, parts, links = prepare(
            products, original, (9, 9), LocalFrame("original-capture"), cells=2
        )
        seen, shared = [], {}
        for key in parts:
            result = select(field, parts, links, [key])
            ids = result["canonical_faces"]
            np.testing.assert_array_equal(
                result["source_faces"], products["terrain_source_faces"][ids]
            )
            self.assertEqual(
                result["vertices"][result["faces"]].tobytes(),
                products["terrain_vertices"][products["terrain_faces"][ids]].tobytes(),
            )
            for local, body in result["body_pairs"]:
                point = result["vertices"][local].tobytes()
                self.assertEqual(point, products["body_vertices"][body].tobytes())
                shared.setdefault(int(body), []).append(key)
            seen.extend(map(int, ids))
        self.assertEqual(sorted(seen), list(range(len(products["terrain_faces"]))))
        self.assertTrue(any(len(keys) > 1 for keys in shared.values()))

    def test_missing_duplicate_and_incompatible_source_requests_decline(self):
        products, original = self.data()
        field, parts, links = prepare(
            products, original, (9, 9), LocalFrame("original-capture"), cells=2
        )
        key = next(iter(parts))
        for chosen in ([key, key], [(999, 999)], []):
            with self.assertRaises(ValueError):
                select(field, parts, links, chosen)
        with self.assertRaises(ValueError):
            cell_groups(np.array([[0, 1, 19]]), (9, 9), 2)
        bad = dict(products, body_vertices=products["body_vertices"].copy())
        bad["body_vertices"][:, 2] += 0.01
        with self.assertRaisesRegex(ValueError, "position words"):
            prepare(bad, original, (9, 9), LocalFrame("original-capture"), cells=2)

    def test_prepared_roundtrip_and_delta_reads_keep_one_shared_body(self):
        products, original = self.data()
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_research."
        ) as tmp:
            root = Path(tmp)
            catalog, _ = pack(
                products, original, np.zeros((9, 9), dtype="<f4"), [0, 0], root, 2
            )
            parts = [tuple(row["key"]) for row in catalog["parts"]]
            requests = (parts[:2], parts[1:3])
            for mode, expected in (("resident", 1), ("compact", 2), ("ready", 3)):
                with patch("shore_contact_parts_study.read", wraps=read) as loads:
                    result = sequence(mode, root, catalog, requests)
                self.assertEqual(
                    sum(
                        call.args[0].name == "body.npz" for call in loads.call_args_list
                    ),
                    1,
                )
                self.assertEqual(len(loads.call_args_list), 1 + expected)
                if mode == "ready":
                    self.assertEqual(
                        [step["payload_files"] for step in result["steps"]], [2, 1]
                    )
                    self.assertEqual(
                        result["steps"][1]["known_compaction_copy_bytes"], 0
                    )
                elif mode == "resident":
                    self.assertEqual(result["steps"][1]["payload_file_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
