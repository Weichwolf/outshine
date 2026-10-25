"""Source area, whole-source fallback, unaveraged response and deferred opaque stops."""

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from build_building_mattes import read_matte
from building_brdf_basis import angular_samples, response_basis
from building_mass_appearance import policy_digest
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_matte_fields import (
    build_fields,
    merge_equal_normal_profiles,
    plane_fields,
    validate_fields,
)
from building_matte_image import cell_profiles, layer_weights, render
from building_matte_planes import cover_domains
from building_tree_transfer import compose_layers, transfers
from compare_mass_appearance import read
from source_material_patches import MaterialPatch
from source_surface_patches import SurfacePatch


def roof(domain, z=1, source=1, ident=0):
    axes = np.eye(3)[:2]
    origin = np.array([0, 0, z], float)
    surface = SurfacePatch(domain, origin, axes, np.array([0, 0, 1.0]), source)
    return MaterialPatch(
        surface,
        {"roof_kind": 1, "ident": ident},
        0,
        1,
        False,
        origin,
        axes,
        {"wall_colour": None},
        1,
    )


def tiny_fields(at):
    patch = roof(shapely.box(0.25, 0.25, 0.75, 0.75))
    cover = {
        "plane_normal": np.array([[0, 0, 1.0]]),
        "plane_offset": np.array([1.0]),
        "patch_planes": np.array([0]),
    }
    return build_fields([patch], cover, np.array([[0, 0, 0], [16, 16, 16.0]]), at)[0]


class BuildingMattes(unittest.TestCase):
    def test_cover_bounds_and_whole_source_fallback(self):
        patches = [roof(shapely.box(0, 0, 1, 1), z=z) for z in (1, 3)]
        bounds = np.array([[0, 0, 0], [4, 4, 4.0]])
        cover, work = cover_domains(patches, bounds)
        self.assertEqual(len(cover["plane_normal"]), 2)
        self.assertLessEqual(
            work["maximum_domain_corner_displacement_m"], float(cover["epsilon"])
        )
        with patch("building_matte_planes.PLANE_LIMIT", 1):
            limited, _ = cover_domains(patches, bounds)
        np.testing.assert_array_equal(limited["source_fallback"], [1])
        np.testing.assert_array_equal(limited["patch_planes"], -1)
        self.assertEqual(len(limited["plane_normal"]), 0)

    def test_subcell_source_area_survives_and_holes_overlap_are_unioned(self):
        at = policy()
        tiny = roof(shapely.box(0.25, 0.25, 0.75, 0.75))
        field, work = plane_fields(
            [tiny], [0], np.array([0, 0, 1.0]), 1, np.zeros(3), 1, at
        )
        np.testing.assert_array_equal(field["alpha"], [0.25])
        self.assertEqual(work["source_projection_union_area_m2"], 0.25)
        domain = shapely.difference(
            shapely.box(0, 0, 2, 2), shapely.box(0.5, 0.5, 1.5, 1.5)
        )
        patches = [roof(domain, source=i, ident=i) for i in (1, 2)]
        field, work = plane_fields(
            patches, [0, 1], np.array([0, 0, 1.0]), 1, np.zeros(3), 2, at
        )
        np.testing.assert_array_equal(field["alpha"], [0.75])
        self.assertEqual(work["source_projection_union_area_m2"], 3)
        self.assertEqual(work["float32_reconstructed_area_m2"], 3)

    def test_deferred_weights_match_independent_composition_and_opaque_stop(self):
        rays = np.array([0, 0, 1, 1])
        depth = np.array([2, 1, 0, 1.0])
        alpha = np.array([0.5, 1, 0.25, 1.0])
        ties = np.arange(4)
        weights, transmission = layer_weights(rays, depth, alpha, ties, 2)
        np.testing.assert_array_equal(weights, [0, 1, 0.25, 0.75])
        colours = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1], [1, 1, 1.0]])
        reference, expected_t, _ = compose_layers(rays, depth, colours, alpha, ties, 2)
        result = np.zeros((2, 3))
        np.add.at(result, rays, weights[:, None] * colours)
        np.testing.assert_array_equal(result, reference)
        np.testing.assert_array_equal(transmission, expected_t)
        with self.assertRaises(ValueError):
            layer_weights(rays, depth, alpha, ties, 1)

    def test_fully_facing_unit_profile_is_exactly_opaque(self):
        data = {
            "alpha": np.ones(1, np.float32),
            "cell_profile_offsets": np.array([0, 4]),
            "profile_normal": np.tile([0, 0, 1.0], (4, 1)),
            "profile_weight": np.array([0.123, 0.234, 0.345, 0.298], np.float32),
        }
        _, fractions, alpha = cell_profiles(
            data, np.array([0]), np.array([[0, 0, 1.0]])
        )
        np.testing.assert_array_equal(alpha, 1)
        np.testing.assert_allclose(
            fractions.sum(axis=1), 1, atol=4 * np.finfo(float).eps
        )

    def test_identical_normal_response_merge_against_current_ggx_basis(self):
        at = policy()
        data = tiny_fields(at)
        palette = at["palette"]
        albedo = np.array([[0.25, 0.75, 0.5], [0.75, 0.25, 0]], np.float32)
        roughness = np.zeros((2, len(palette)), np.float32)
        roughness[0, 0], roughness[1, -1] = 1, 1
        data.update(
            {
                "profile_normal": np.array([[0, 0, 1]] * 2, np.float32),
                "profile_material": np.array([0, 1], np.uint32),
                "profile_weight": np.array([0.5, 0.5], np.float32),
                "cell_profile_offsets": np.array([0, 2], np.uint32),
                "material_albedo": albedo,
                "material_roughness_weights": roughness,
            }
        )
        merged, work = merge_equal_normal_profiles(data)
        self.assertEqual(work["profiles_after"], 1)
        np.testing.assert_array_equal(merged["alpha"], data["alpha"])
        tables = read(
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/material-filter/energy-tables.npz"
        )
        basis = response_basis(palette, tables, angular_samples())
        specular, diffuse = basis["specular"], basis["diffuse"]
        expected = sum(
            0.5
            * (
                (0.1 + diffuse[:, None]) * albedo[i]
                + (specular @ roughness[i])[:, None]
            )
            for i in range(2)
        )
        material = int(merged["profile_material"][0])
        actual = (0.1 + diffuse[:, None]) * merged["material_albedo"][material] + (
            specular @ merged["material_roughness_weights"][material]
        )[:, None]
        np.testing.assert_allclose(
            actual,
            expected,
            atol=64 * np.finfo(float).eps,
            rtol=64 * np.finfo(float).eps,
        )
        different = data["profile_normal"].copy()
        different[1, 0] = np.nextafter(np.float32(0), np.float32(1))
        unchanged, work = merge_equal_normal_profiles(
            data | {"profile_normal": different}
        )
        self.assertEqual(work["profiles_after"], 2)
        np.testing.assert_array_equal(unchanged["profile_normal"], different)

    def test_bound_reader_does_not_generate_and_rejects_wrong_parent(self):
        at = policy()
        data = tiny_fields(at)
        tree = {
            "basis": np.eye(3),
            "anchor": np.zeros(3),
            "tree_sha256": np.arange(32, dtype=np.uint8),
        }
        with tempfile.TemporaryDirectory(prefix="outshine-matte-reader-") as directory:
            root = Path(directory)
            node = root / "node.npz"
            node.write_bytes(b"immutable source")
            data.update(
                tree,
                needs_sources=np.array([3], np.uint32),
                node_sha256=np.frombuffer(
                    hashlib.sha256(node.read_bytes()).digest(), np.uint8
                ),
                policy_sha256=np.frombuffer(policy_digest(at), np.uint8),
            )
            np.savez(root / "matte.npz", **data)
            with patch(
                "build_building_mattes.build_case",
                side_effect=AssertionError("loading must not generate"),
            ):
                loaded = read_matte(root / "matte.npz", node, tree, at)
            np.testing.assert_array_equal(loaded["alpha"], [0.25])
            node.write_bytes(b"other source")
            with self.assertRaisesRegex(ValueError, "differs"):
                read_matte(root / "matte.npz", node, tree, at)
        with self.assertRaises(ValueError):
            validate_fields(
                data | {"profile_weight": np.zeros_like(data["profile_weight"])}
            )

    def test_empty_matte_preserves_entire_native_remainder(self):
        at = policy()
        data = tiny_fields(at)
        for key in (
            "plane_origin",
            "plane_axes",
            "plane_low",
            "plane_shape",
            "alpha",
            "profile_normal",
            "profile_material",
            "profile_weight",
            "material_albedo",
            "material_roughness_weights",
        ):
            data[key] = data[key][:0]
        data.update(
            plane_cell_offsets=np.array([0], np.uint32),
            cell_profile_offsets=np.array([0], np.uint32),
            needs_sources=np.array([1], np.uint32),
        )
        validate_fields(data)
        original = {
            "positions": np.array([[-10, -10, 1], [0, 10, 1], [10, -10, 1.0]]),
            "indices": np.array([[0, 1, 2]]),
            "uv": np.zeros((3, 2)),
            "colour": np.ones((3, 4)),
            "binding": np.array([[0, 1, 0, 0, 0, 0]]),
        }
        fields, needs = face_materials(original, at)
        self.assertFalse(needs)
        camera = (np.zeros(3), np.diag([-1, 1, 1]), 2, 1, 1)
        box = np.array([[0, 0], [1, 1]])
        tables = read(
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/material-filter/energy-tables.npz"
        )
        product = {
            "indices": np.empty((0, 3), int),
            "needs_sources": np.array([1]),
            "group_coverage": np.empty((0, 3)),
        }
        appearance = {
            "roughness_palette": at["palette"],
            "supported": np.empty(0, bool),
            "albedo": np.empty((0, 2, 3)),
        }
        colours = []
        for lighting in ({}, {"light": [0, 0, -1], "intensity": 2}, {"intensity": 0}):
            actual, actual_work = render(
                data, original, fields, camera, box, tables, rate=4, **lighting
            )
            expected, expected_work = evaluate(
                original,
                original,
                fields,
                appearance,
                product,
                camera,
                box,
                tables,
                supersample=4,
                **lighting,
            )
            control, control_work = transfers(
                original,
                original,
                fields,
                product,
                appearance,
                camera,
                box,
                tables,
                4,
                **lighting,
            )
            for key in ("C", "T"):
                np.testing.assert_array_equal(actual[key], expected[key])
                np.testing.assert_array_equal(control[key], expected[key])
            if lighting.get("intensity") == 0:
                for work in (actual_work, expected_work, control_work):
                    self.assertEqual(work["ggx_lobe_evaluations"], 0)
            colours.append(actual["C"])
        self.assertGreater(float(np.max(colours[1] - colours[2])), 0)


if __name__ == "__main__":
    unittest.main()
