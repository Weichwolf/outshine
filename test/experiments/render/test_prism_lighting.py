"""Ready-solid diagnostic triangles, explicit identities and no-GEOS loading."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from attributed_prisms import (
    compile_product,
    load_product,
    material_tables,
    render_product,
)
from compact_attributed_prisms import compact
from native_capture_meshes import independent_depth
from packed_normals import pack
from primitive_bvh import build, trace
from prism_diagnostic_mesh import represented_mesh
from test_attributed_prisms import example


def empty_fallback():
    return {
        "positions": np.empty((0, 3), np.float32),
        "indices": np.empty((0, 3), np.uint32),
        "normal": np.empty(0, np.uint32),
        "colour": np.empty((0, 4), np.float32),
        "binding": np.empty((0, 6), np.int32),
    }


class PrismLighting(unittest.TestCase):
    def test_diagnostic_faces_match_ready_clip_and_normals(self):
        prisms, _ = example()
        hierarchy = build(prisms.bounds)
        mesh, residual = represented_mesh(
            (prisms, None, hierarchy, empty_fallback(), None)
        )
        self.assertLessEqual(residual, 1e-7)
        o = np.array([[104, 202, 320], [95, 202, 314], [104, 190, 314]], float)
        d = np.array([[0, 0, -1], [1, 0, 0], [0, 1, 0]], float)
        expected, owner, face, _ = trace(prisms, hierarchy, o, d)
        actual = independent_depth(mesh["positions"][mesh["indices"]], o, d)
        np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-8)
        np.testing.assert_array_equal(mesh["source_id"], 11)
        normals = mesh["shading_normal"][mesh["indices"][:, 0]]
        np.testing.assert_array_equal(normals, prisms.planes[mesh["plane_owner"], :3])
        self.assertTrue((owner == 0).all())
        self.assertTrue((face >= 0).all())

    def test_compact_rows_keep_dense_hits_and_do_not_expand_on_load(self):
        prisms, recipes = example()
        materials = material_tables(prisms, recipes)
        hierarchy = build(prisms.bounds)
        fallback = empty_fallback() | {
            "uv": np.empty((0, 2), np.float32),
            "metres": np.empty((0, 2), np.float32),
        }
        arrays = {
            **{"prism_" + k: v for k, v in vars(prisms).items()},
            **{"material_" + k: v for k, v in vars(materials).items()},
            **{"fallback_" + k: v for k, v in fallback.items()},
            "node_bounds": hierarchy.bounds,
            "node_children": hierarchy.children,
        }
        o = np.array([[104, 190, 314], [104, 202, 320], [104, 202, 300]], float)
        d = np.array([[0, 1, 0], [0, 0, -1], [0, 0, 1]], float)
        with tempfile.TemporaryDirectory(
            prefix="outshine-prism-layout-test-"
        ) as directory:
            values = []
            for name, data in [("dense", arrays), ("compact", compact(arrays))]:
                path = Path(directory) / (name + ".npz")
                np.savez_compressed(path, **data)
                product = load_product(path)
                values.append(render_product(product, o, d))
                if name == "compact":
                    self.assertEqual(len(product[1].edge), 4)
                    self.assertEqual(len(product[1].bays), 4)
                    self.assertEqual(product[0].bounds.nbytes, 0)
                    self.assertEqual(product[0].roof.nbytes, 0)
            for index in [0, 2, 3, 4, 5, 6]:
                np.testing.assert_array_equal(values[0][index], values[1][index])
            for key in values[0][1]:
                np.testing.assert_array_equal(values[0][1][key], values[1][1][key])

    def test_fallback_only_keeps_source_rgba_and_ready_reader(self):
        source = {
            "positions": np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1]], np.float32),
            "indices": np.array([[0, 2, 1]], np.uint32),
            "uv": np.zeros((3, 2), np.float32),
            "normal": np.repeat(pack(np.array([[0, 0, -1.0]])), 3),
            "metres": np.zeros((3, 2), np.float32),
            "colour": np.tile(np.array([0.2, 0.4, 0.6, 0.5], np.float32), (3, 1)),
            "binding": np.array([[0, 99, -1, -1, 0, 0]], np.int32),
        }
        arrays, _ = compile_product(
            {"sources": [{"index": 99, "surface": None}]}, source, {99: None}
        )
        with tempfile.TemporaryDirectory(
            prefix="outshine-prism-ready-test-"
        ) as directory:
            path = Path(directory) / "product.npz"
            np.savez_compressed(path, **compact(arrays))
            product = load_product(path)
            mesh, _ = represented_mesh(product)
            np.testing.assert_array_equal(mesh["colour"], source["colour"])
            np.testing.assert_array_equal(mesh["source_id"], [99])
            code = """import sys,json
sys.modules['shapely']=None
sys.modules['prepared_surface_recipes']=None
from attributed_prisms import load_product,render_product
import numpy as np
p=load_product(sys.argv[1]);r=render_product(p,np.array([[.2,.2,0]]),np.array([[0,0,1.0]]))
print(json.dumps({'depth':r[0].tolist(),'source':r[2].tolist(),'colour':r[1]['colour'].tolist(),'edge_rows':len(p[1].edge)}))
"""
            value = json.loads(
                subprocess.check_output(
                    [sys.executable, "-B", "-W", "error", "-c", code, str(path)],
                    text=True,
                    env=os.environ,
                )
            )
            self.assertEqual(value["depth"], [1.0])
            self.assertEqual(value["source"], [99])
            self.assertEqual(value["edge_rows"], 0)
            np.testing.assert_array_equal(value["colour"], source["colour"][:1])


if __name__ == "__main__":
    unittest.main()
