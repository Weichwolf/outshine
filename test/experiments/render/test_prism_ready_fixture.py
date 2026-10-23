"""Shared original-derived ready input and independent exact native field control."""

import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from full_view_reference import load, views
from packed_normals import pack
from prepared_surface_recipes import load_recipes
from primitive_bvh import Hierarchy, trace
from roof_prisms import Prisms
from triangle_image_reference import image_rays


class ReadyFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view"
        )
        cls.ready = cls.root / "prism-ready"
        cls.manifest = json.loads((cls.ready / "manifest.json").read_text())
        cls.source, cls.document, cls.context, _ = load(cls.root / "cell.json.gz")

    def test_manifest_binds_unchanged_ready_arrays_to_original_input(self):
        manifest = self.manifest
        for name, entry in manifest["files"].items():
            data = (self.ready / name).read_bytes()
            self.assertEqual(len(data), entry["bytes"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"])
            with np.load(self.ready / name, allow_pickle=False) as arrays:
                self.assertEqual(
                    sum(arrays[key].nbytes for key in arrays.files),
                    manifest["array_payload_bytes"][name],
                )
        for name, key in [
            ("manifest.json", "original_manifest_sha256"),
            ("original-inputs.tar.gz", "original_archive_sha256"),
        ]:
            self.assertEqual(
                hashlib.sha256((self.root / name).read_bytes()).hexdigest(),
                manifest[key],
            )

    def test_complete_BSC3_partition_and_exact_fallback(self):
        recipes, _ = load_recipes(self.root / "cell.json.gz", self.document)
        source = self.source
        with np.load(self.ready / "product.npz", allow_pickle=False) as arrays:
            analytic = set(arrays["prism_binding"][:, 0])
            fallback = set(arrays["fallback_source_id"])
            self.assertFalse(analytic & fallback)
            self.assertEqual(analytic | fallback, set(recipes))
            self.assertEqual(analytic | fallback, set(source["binding"][:, 1]))
            self.assertEqual((len(analytic), len(fallback)), (557, 19))
            faces = arrays["prism_fallback_faces"]
            self.assertEqual(len(faces), 887)
            np.testing.assert_array_equal(
                arrays["fallback_source_id"], source["binding"][faces, 1]
            )
            corners = arrays["fallback_indices"]
            for stored, original in [
                ("positions", "positions"),
                ("uv", "uv"),
                ("normal", "normal"),
                ("metres", "metres"),
            ]:
                np.testing.assert_array_equal(
                    arrays["fallback_" + stored][corners],
                    source[original][source["indices"][faces]],
                )

    def test_exact_control_roundtrips_every_original_corner_and_source(self):
        source = self.source
        inputs = native_inputs(source)
        indices = source["indices"]
        attributes = np.column_stack((inputs["uv"], source["metres"]))
        self.assertTrue(np.all(source["colour"] == 1))
        with np.load(self.ready / "exact-control.npz", allow_pickle=False) as arrays:
            np.testing.assert_array_equal(
                arrays["positions"][arrays["position_indices"]],
                source["positions"][indices].view(np.uint32),
            )
            np.testing.assert_array_equal(
                arrays["attributes"][arrays["attribute_indices"]],
                attributes[indices].view(np.uint32),
            )
            lookup = arrays["normal_code_index"]
            np.testing.assert_array_equal(
                arrays["normal_table"][lookup], source["normal"][indices[:, 0]]
            )
            np.testing.assert_array_equal(
                arrays["code_table"][lookup], inputs["flat_codes"]
            )
            owner = np.repeat(arrays["source_ids"], np.diff(arrays["source_offsets"]))
            np.testing.assert_array_equal(owner, source["binding"][:, 1])
            self.assertEqual(len(owner), 14169)

    def test_fit_preserves_identity_material_and_fallback_arrays(self):
        changed = {"prism_planes", "node_bounds", "node_children"}
        with (
            np.load(self.ready / "product.npz", allow_pickle=False) as baseline,
            np.load(self.ready / "plane-fit.npz", allow_pickle=False) as fitted,
        ):
            self.assertEqual(
                set(fitted.files), set(baseline.files) | {"fit_normal_words"}
            )
            for key in baseline.files:
                self.assertEqual(fitted[key].dtype, baseline[key].dtype)
                self.assertEqual(fitted[key].shape, baseline[key].shape)
                if key not in changed:
                    self.assertEqual(
                        fitted[key].tobytes(), baseline[key].tobytes(), key
                    )
            np.testing.assert_array_equal(
                fitted["fit_normal_words"], pack(baseline["prism_planes"][:, :3])
            )
            self.assertFalse(
                np.array_equal(fitted["prism_planes"], baseline["prism_planes"])
            )

    def test_fixed_fitted_input_selects_both_known_original_materials(self):
        poses = views(self.document, self.context)
        cases = [
            ("moving-023", [[346, 285], [401, 303]], 2553, 3024, 133),
            ("static-025", [[155, 285], [223, 304]], 2678, 2472, -994),
        ]
        with np.load(self.ready / "plane-fit.npz", allow_pickle=False) as arrays:
            prism = Prisms(
                **{
                    k.removeprefix("prism_"): arrays[k]
                    for k in arrays.files
                    if k.startswith("prism_")
                }
            )
            hierarchy = Hierarchy(arrays["node_bounds"], arrays["node_children"])
            for name, region, index, identity, expected_code in cases:
                with self.subTest(pose=name):
                    size = np.diff(region, axis=0)[0]
                    origins, directions = image_rays(
                        poses[name], np.array(region), *size, 2
                    )
                    origin, direction = (
                        origins[index : index + 1],
                        directions[index : index + 1],
                    )
                    depth, owner, plane, _ = trace(prism, hierarchy, origin, direction)
                    self.assertTrue(np.isfinite(depth[0]))
                    self.assertEqual(prism.binding[owner[0], 0], identity)
                    frame = arrays["material_frames"][owner[0]]
                    point = origin[0] + depth[0] * direction[0]
                    gable = (point - frame[0]) @ frame[3] > arrays["material_heights"][
                        owner[0], 3
                    ]
                    pair = arrays["material_codes"][
                        arrays["material_code_index"][plane[0]]
                    ]
                    self.assertEqual(pair[int(gable)], expected_code)
