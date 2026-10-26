"""Projected work bounds, state receipts and nonlinear normal counterexample."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_parent_shading_cases import normal_counter
from building_parent_shading_work import factor_work, projected_work
from compare_mass_appearance import read
from fit_building_brdf import validate_fit_receipt
from fit_building_parent_shading import compile_states

TABLES = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/material-filter/energy-tables.npz"
)


class BuildingParentShading(unittest.TestCase):
    def test_offline_fitter_does_not_load_image_or_mesh_dependencies(self):
        code = (
            "import sys; sys.path.insert(0, " + repr(str(Path(__file__).parent)) + "); "
            "import fit_building_parent_shading; "
            "assert not any(n in sys.modules for n in ('trimesh', 'scipy', 'PIL'))"
        )
        result = subprocess.run(
            [sys.executable, "-B", "-W", "error", "-c", code],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_reference_work_factorization_is_multiplicative(self):
        factors = factor_work(
            {"hit_samples": 2, "ggx_lobe_evaluations": 4},
            {"hit_samples": 4, "ggx_lobe_evaluations": 12},
            {"ggx_lobe_evaluations": 18},
        )
        product = (
            factors["proxy_first_hit_sample_factor"]
            * factors["first_hit_average_lobe_factor"]
            * factors["additional_layers_ggx_factor"]
        )
        self.assertEqual(product, 4.5)
        self.assertEqual(product, factors["total_reference_ggx_factor"])

    def test_projected_triangle_box_quad_slots_and_culling(self):
        mesh = {
            "positions": np.array([[-1, -1, 0], [1, -1, 0], [0, 1, 0]], np.float32),
            "indices": np.array([[0, 1, 2]], np.uint16),
        }
        camera = (np.array([0, 0, 10]), np.diag([1, 1, -1]), 10, 10, 10)
        work = projected_work(mesh, camera, np.array([3]))
        self.assertEqual(work["triangle_bbox_pixel_slots_before_depth"], 4)
        self.assertEqual(work["triangle_bbox_2x2_quad_slots_before_depth"], 4)
        self.assertEqual(work["logical_lobe_quad_slots_before_depth"], 12)
        mesh["indices"] = mesh["indices"][:, ::-1]
        self.assertEqual(
            projected_work(mesh, camera, np.array([3]))[
                "front_facing_reference_triangles"
            ],
            0,
        )
        mesh["indices"] = mesh["indices"][:, ::-1]
        mesh["positions"][:, 0] += 100
        self.assertEqual(
            projected_work(mesh, camera, np.array([3]))[
                "triangle_bbox_pixel_slots_before_depth"
            ],
            0,
        )
        with self.assertRaisesRegex(ValueError, "far geometry"):
            projected_work(mesh, camera, np.array([-1]))

    def test_material_compiler_and_receipts_do_not_decode_geometry(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-parent-shading-"
        ) as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "report.json").write_text(
                json.dumps({"cells": [1], "node_files": {"0": "node.npz"}})
            )
            weights = np.array([[[0.2, 0.3, 0.5], [0.5, 0.2, 0.3]]], np.float32)
            np.savez(
                source / "node.npz",
                m__roughness_weights=weights,
                m__albedo=np.full((1, 2, 3), 0.5, np.float32),
                m__roughness_palette=np.array([0.12, 0.78, 0.91], np.float32),
                m__supported=np.ones(1, bool),
                g__positions=np.zeros((3, 3)),
            )
            with np.load(source / "node.npz", allow_pickle=False) as archive:
                cls = type(archive)
            original = cls.__getitem__

            def getitem(archive, key):
                self.assertFalse(key.startswith("g__"))
                return original(archive, key)

            with patch.object(cls, "__getitem__", getitem):
                result = compile_states(source, root / "fits", TABLES)
            self.assertEqual(result["shared_response_palettes"], 1)
            self.assertEqual(
                result["raw_source_container_bytes_hashed_once"],
                (source / "node.npz").stat().st_size,
            )
            for budget in (1, 2):
                validate_fit_receipt(root / f"fits/{budget}", budget, TABLES)
                with np.load(root / f"fits/{budget}/node.npz") as data:
                    self.assertTrue(
                        (np.count_nonzero(data["weights"], axis=-1) <= budget).all()
                    )
            path = root / "fits/2/report.json"
            receipt = json.loads(path.read_text())
            del receipt["response_program_sha256"]["material_filter.py"]
            path.write_text(json.dumps(receipt))
            with self.assertRaisesRegex(ValueError, "program reference"):
                validate_fit_receipt(root / "fits/2", 2, TABLES)

    def test_normal_average_is_not_a_valid_shading_replacement(self):
        case = normal_counter(
            np.array([0.4, 0.5, 0.6]),
            np.array([1.0], np.float32),
            np.array([0.12], np.float32),
            read(TABLES),
        )
        reference = np.array(case["equal_texel_response"])
        averaged = np.array(case["normalized_mean_response"])
        self.assertTrue((reference > 2 * averaged).all())


if __name__ == "__main__":
    unittest.main()
