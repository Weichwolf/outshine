"""Exercise the actual asset-render client with independent glTF/GLB inputs."""
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / "build/outshine-client"


class AssetRender(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="outshine-client-render-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        positions = [-2, -.5, 0, 2, -.5, 0, 2, .5, 0, -2, -.5, 0, 2, .5, 0, -2, .5, 0]
        self.data = struct.pack("<18f", *positions)
        self.asset = {
            "asset": {"version": "2.0"}, "extensionsUsed": ["KHR_materials_unlit"],
            "buffers": [{"uri": "mesh.bin", "byteLength": len(self.data)}],
            "bufferViews": [{"buffer": 0, "byteLength": len(self.data)}],
            "accessors": [{"bufferView": 0, "componentType": 5126, "count": 6,
                           "type": "VEC3", "min": [-2, -.5, 0], "max": [2, .5, 0]}],
            "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1]},
                           "extensions": {"KHR_materials_unlit": {}}}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}],
            "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
        }
        self.write_asset()

    def write_asset(self):
        (self.root / "scene.gltf").write_text(json.dumps(self.asset))
        (self.root / "mesh.bin").write_bytes(self.data)

    def run_client(self, *args, invalid_video=False):
        env = dict(os.environ)
        if invalid_video:
            env["SDL_VIDEODRIVER"] = "outshine-intentionally-unavailable"
        return subprocess.run([str(CLIENT), "render", *map(str, args)], cwd=ROOT, env=env,
                              capture_output=True, text=True, timeout=60)

    def render(self, extent="256x128", *options, asset="scene.gltf", output="capture.png"):
        path = self.root / output
        result = self.run_client(self.root / asset, extent, path, "--lighting", "authored", *options)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(path.is_file())
        pixels = np.asarray(Image.open(path).convert("RGB"))
        self.assertEqual(pixels.shape[:2], tuple(reversed(tuple(map(int, extent.split("x"))))))
        return pixels

    def test_auto_framing_covers_both_viewport_axes(self):
        for extent in ("256x128", "128x256", "128x128"):
            with self.subTest(extent=extent):
                pixels = self.render(extent)
                y, x = np.where(pixels[..., 0] > 128)
                self.assertGreater(len(x), 0, "frame cannot be empty")
                self.assertGreater(x.min(), 0)
                self.assertLess(x.max(), pixels.shape[1] - 1)
                self.assertGreater(y.min(), 0)
                self.assertLess(y.max(), pixels.shape[0] - 1)
                self.assertEqual(np.max(pixels[..., 1:]), 0)

    def test_glb_and_gltf_render_identically(self):
        glb = copy.deepcopy(self.asset)
        del glb["buffers"][0]["uri"]
        encoded = json.dumps(glb).encode()
        encoded += b" " * (-len(encoded) % 4)
        binary = self.data + b"\0" * (-len(self.data) % 4)
        (self.root / "scene.glb").write_bytes(
            struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary)) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(binary), 0x004E4942) + binary)
        np.testing.assert_array_equal(self.render(), self.render(asset="scene.glb", output="glb.png"))

    def add_camera(self):
        self.asset["cameras"] = [{"type": "perspective", "perspective": {"yfov": 1, "znear": .1}}]
        self.asset["nodes"] = [{"children": [1, 2]}, {"mesh": 0}, {"camera": 0, "translation": [0, 0, 5]}]
        self.write_asset()

    def test_authored_camera_and_overrides(self):
        self.add_camera()
        default = self.render("256x128")
        np.testing.assert_array_equal(default, self.render("256x128", "--camera", "0"))
        automatic = self.render("256x128", "--camera", "auto")
        self.assertFalse(np.array_equal(default, automatic))
        explicit = self.render("256x128", "--position", "0,0,7", "--look-at", "0,0,0", "--fov", "50")
        self.assertTrue(np.any(explicit[..., 0] > 128))
        self.assertFalse(np.array_equal(default, explicit))

    def test_camera_and_geometry_share_exact_time(self):
        self.add_camera()
        offset = len(self.data)
        self.data += struct.pack("<8f", 0, 1, 0, 0, 0, 2, 0, 0)
        self.asset["buffers"][0]["byteLength"] = len(self.data)
        self.asset["bufferViews"] += [{"buffer": 0, "byteOffset": offset, "byteLength": 8},
                                      {"buffer": 0, "byteOffset": offset + 8, "byteLength": 24}]
        self.asset["accessors"] += [{"bufferView": 1, "componentType": 5126, "count": 2,
                                     "type": "SCALAR", "min": [0], "max": [1]},
                                    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "VEC3"}]
        self.asset["animations"] = [{"samplers": [{"input": 1, "output": 2}],
                                     "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]}]
        self.write_asset()
        np.testing.assert_array_equal(
            self.render("256x128", "--camera", "0", "--time", ".25"),
            self.render("256x128", "--camera", "0", "--time", ".75"))

    def test_orthographic_camera_and_variant_selection(self):
        self.add_camera()
        self.asset["cameras"] = [{"type": "orthographic", "orthographic": {
            "xmag": 2.5, "ymag": 1.5, "znear": .1, "zfar": 10}}]
        self.asset["extensionsUsed"].append("KHR_materials_variants")
        self.asset["extensions"] = {"KHR_materials_variants": {"variants": [{"name": "blue"}]}}
        blue = copy.deepcopy(self.asset["materials"][0])
        blue["pbrMetallicRoughness"]["baseColorFactor"] = [0, 0, 1, 1]
        self.asset["materials"].append(blue)
        self.asset["meshes"][0]["primitives"][0]["extensions"] = {
            "KHR_materials_variants": {"mappings": [{"material": 1, "variants": [0]}]}}
        self.write_asset()
        red = self.render("256x128", "--camera", "0")
        selected = self.render("256x128", "--camera", "0", "--variant", "blue")
        np.testing.assert_array_equal(red[..., 0], selected[..., 2])
        self.assertTrue(np.any(selected[..., 2] > 128))
        self.assertEqual(np.max(selected[..., :2]), 0)
        invalid = self.run_client(self.root / "scene.gltf", "128x128", self.root / "bad.png",
                                  "--variant", "unknown")
        self.assertNotEqual(invalid.returncode, 0)

    def test_invalid_arguments_precede_platform_start(self):
        common = [self.root / "scene.gltf", "128x128", self.root / "bad.png"]
        cases = [[], common[:2], common + ["--time"], common + ["--bogus", "1"],
                 common + ["--time", "nan"], common + ["--time", "-1"],
                 common + ["--time", "1junk"], common + ["--camera", "-1"],
                 common + ["--animation", "1.5"], common + ["--exposure", "0"],
                 common + ["--fov", "180"], common + ["--position", "1,2,3"],
                 common + ["--position", "1,2,3", "--look-at", "1,2,3"],
                 common + ["--time", "0", "--time", "1"]]
        cases += [[common[0], extent, common[2]] for extent in ("0x1", "1x0", "1x-1", "1x1junk", "4097x1", "1", "1x2x3")]
        for args in cases:
            with self.subTest(args=args):
                result = self.run_client(*args, invalid_video=True)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertNotIn("outshine-intentionally-unavailable", result.stderr)
        self.assertFalse((self.root / "bad.png").exists())

    def test_invalid_asset_camera_clip_and_output_fail(self):
        for asset, options in (("absent.gltf", []), ("scene.gltf", ["--camera", "0"]),
                               ("scene.gltf", ["--animation", "0"])):
            result = self.run_client(self.root / asset, "128x128", self.root / "bad.png", *options)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(result.stderr)
        result = self.run_client(self.root / "scene.gltf", "128x128", self.root)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(result.stderr)


if __name__ == "__main__":
    unittest.main()
