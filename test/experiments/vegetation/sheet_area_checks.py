"""Analytic two-sided R/T, real opaque blockers, covariance and early demand controls."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from needle_area_checks import sheet
from scatter_scene import ScatterScene
from sheet_leaf_render import read_source, shading
from sheet_light_measure import SheetMeasure
from surface_area_light import integrate


def thin_sheet(optical=(0.08, 0.22, 0.025, 0.035, 0.12, 0.018)):
    source = sheet()
    source["optical"] = np.tile(optical, (4, 1))
    source["surface_model"] = "thin-sheet"
    return source


def measure(source, view=(0.0, 0, 1), size=4, order=2):
    view = np.array(view)
    scene = ScatterScene(source["vertices"], source["faces"])
    collector = SheetMeasure(source, scene, view, [[0, 0, 1], [0, 0, -1]], size=size)
    direct, work = integrate(
        source,
        scene,
        view,
        [view],
        size=size,
        order=order,
        contact_measure=collector,
        two_sided=True,
    )
    np.testing.assert_array_equal(direct[0].ravel(), collector.coverage)
    return collector, scene, work


class SheetAreaTests(unittest.TestCase):
    def test_prepared_reader_keeps_exact_arrays_and_checks_semantics_and_bytes_before_decode(
        self,
    ):
        source = thin_sheet()
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-sheet-reader-", dir="/tmp"
        ) as folder:
            path = Path(folder)
            np.savez_compressed(
                path / "source.npz",
                **{k: v for k, v in source.items() if isinstance(v, np.ndarray)},
            )
            descriptor = {
                "schema": "selected-thin-leaves-1",
                "surface_model": "thin-sheet",
                "source_sha256": hashlib.sha256(
                    (path / "source.npz").read_bytes()
                ).hexdigest(),
            }
            (path / "source.json").write_text(json.dumps(descriptor))
            actual, _ = read_source(path)
            for key in ("vertices", "faces", "normals", "optical"):
                np.testing.assert_array_equal(actual[key], source[key])
            descriptor["surface_model"] = "closed-solid"
            (path / "source.json").write_text(json.dumps(descriptor))
            with (
                patch(
                    "sheet_leaf_render.np.load", side_effect=AssertionError("decode")
                ),
                self.assertRaises(ValueError),
            ):
                read_source(path)
            descriptor["surface_model"] = "thin-sheet"
            descriptor["source_sha256"] = "0" * 64
            (path / "source.json").write_text(json.dumps(descriptor))
            with (
                patch(
                    "sheet_leaf_render.np.load", side_effect=AssertionError("decode")
                ),
                self.assertRaises(ValueError),
            ):
                read_source(path)

    def test_exactly_edge_on_sheet_has_no_projected_mass_or_light_queries(self):
        result, scene, work = measure(thin_sheet(), (1.0, 0, 0))
        np.testing.assert_array_equal(result.coverage, 0)
        np.testing.assert_array_equal(result.joint, 0)
        self.assertEqual(scene.work["rays"], 0)
        self.assertEqual(work["sampled_points"], 0)

    def test_primary_renderer_uses_existing_two_sided_response_and_energy(self):
        source = thin_sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        points = np.array([[0.25, 0.25, 0], [0.75, 0.75, 0]])
        rgb, reflection = shading(
            source,
            scene,
            points,
            np.array([0, 1]),
            np.array([0.0, 0, 1]),
            np.array([[0.0, 0, 1], [0, 0, -1]]),
        )
        energy = np.array([3.0, 2.7, 2.4])
        np.testing.assert_allclose(
            rgb[0],
            np.tile(source["optical"][0, :3] * energy / np.pi, (2, 1)),
            atol=1e-14,
        )
        np.testing.assert_allclose(
            rgb[1],
            np.tile(source["optical"][0, 3:] * energy / np.pi, (2, 1)),
            atol=1e-14,
        )
        np.testing.assert_array_equal(reflection[0], rgb[0])
        np.testing.assert_array_equal(reflection[1], 0)

    def test_constant_sheet_has_analytic_reflection_and_transmission_on_both_sides(
        self,
    ):
        source = thin_sheet()
        optical = source["optical"][0]
        for view in ((0.0, 0, 1), (0.0, 0, -1)):
            result, scene, work = measure(source, view)
            reflection_index = int(view[2] < 0)
            for light in (0, 1):
                expected = optical[:3] if light == reflection_index else optical[3:]
                np.testing.assert_allclose(
                    result.joint[light],
                    result.coverage[:, None] * expected / np.pi,
                    atol=1e-14,
                )
            np.testing.assert_allclose(result.unshadowed, result.joint, atol=1e-14)
            np.testing.assert_allclose(
                result.reflection[1 - reflection_index], 0, atol=1e-14
            )
            self.assertEqual(scene.work["rays"], work["sampled_points"] + result.points)
            self.assertEqual(result.work["reused_coaxial_active"], result.points)

    def test_reversed_winding_and_authored_normals_preserve_symmetric_sheet_result(
        self,
    ):
        source = thin_sheet()
        base, _, _ = measure(source, (0.0, 0, -1))
        source["faces"] = source["faces"][:, ::-1].copy()
        source["normals"] = -source["normals"][:, ::-1].copy()
        reversed_result, _, _ = measure(source, (0.0, 0, -1))
        np.testing.assert_allclose(reversed_result.coverage, base.coverage, atol=1e-14)
        np.testing.assert_allclose(reversed_result.joint, base.joint, atol=1e-14)

    def test_actual_opaque_neighbour_blocks_transmission_with_either_winding(self):
        for reverse in (False, True):
            source = thin_sheet()
            source["vertices"] = np.r_[
                source["vertices"], source["vertices"] - [0, 0, 0.1]
            ]
            blocker = source["faces"] + 4
            source["faces"] = np.r_[
                source["faces"], blocker[:, ::-1] if reverse else blocker
            ]
            source["normals"] = np.tile([0.0, 0, 1], (4, 3, 1))
            source["optical"] = np.r_[source["optical"], np.zeros((4, 6))]
            result, _, _ = measure(source)
            np.testing.assert_array_equal(result.joint[1], 0)
            self.assertGreater(result.unshadowed[1, :, 1].sum(), 0.4)
            np.testing.assert_allclose(
                result.joint[0], result.unshadowed[0], atol=1e-14
            )

    def test_prepared_optics_and_cosines_cannot_be_averaged_independently(self):
        source = thin_sheet()
        source["optical"] = np.tile([0.2, 0.2, 0.2, 0.7, 0.7, 0.7], (4, 1))
        source["optical"][0] = [0.8, 0.8, 0.8, 0.1, 0.1, 0.1]
        scene = ScatterScene(source["vertices"], source["faces"])
        result = SheetMeasure(source, scene, [0, 0, 1], [[0, 0, 1], [0, 0, -1]], size=1)
        result.add(
            np.array([[0.0, 0, 0], [1.0, 1, 0]]),
            np.zeros(2, int),
            np.full(2, 0.5),
            np.array([[np.sqrt(0.96), 0, 0.2], [0.2, 0, np.sqrt(0.96)]]),
            face_ids=np.zeros(2, int),
        )
        self.assertAlmostEqual(
            result.joint[0, 0, 0], (0.8 * 0.2 + 0.2 * np.sqrt(0.96)) / (2 * np.pi)
        )
        self.assertAlmostEqual(
            result.joint[1, 0, 0], (0.1 * 0.2 + 0.7 * np.sqrt(0.96)) / (2 * np.pi)
        )
        separate = result.separate_unshadowed()
        self.assertAlmostEqual(
            separate[0, 0, 0], 0.5 * (0.2 + np.sqrt(0.96)) / (2 * np.pi)
        )
        self.assertGreater(abs(separate[0, 0, 0] - result.joint[0, 0, 0]), 0.03)

    def test_black_response_and_zero_mass_skip_light_rays_without_dropping_coverage(
        self,
    ):
        result, scene, work = measure(thin_sheet(np.zeros(6)))
        np.testing.assert_array_equal(result.joint, 0)
        self.assertGreater(result.coverage.sum(), 0)
        self.assertEqual(scene.work["rays"], work["sampled_points"])
        before = scene.work["rays"]
        with patch(
            "sheet_light_measure.contact_optics", side_effect=AssertionError("optics")
        ):
            result.add(
                np.zeros((1, 3)),
                np.zeros(1, int),
                np.zeros(1),
                np.array([[0.0, 0, 1]]),
                face_ids=np.zeros(1, int),
            )
        self.assertEqual(scene.work["rays"], before)

    def test_wrong_semantics_and_exhausted_work_refuse_before_queries(self):
        source = thin_sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        result = SheetMeasure(source, scene, [0, 0, 1], [[0, 0, 1]], size=1, max_work=1)
        with self.assertRaises(ValueError):
            integrate(
                source,
                scene,
                np.array([0.0, 0, 1]),
                [[0, 0, 1]],
                size=1,
                contact_measure=result,
            )
        with (
            patch(
                "sheet_light_measure.contact_optics",
                side_effect=AssertionError("optics"),
            ),
            self.assertRaises(ValueError),
        ):
            result.add(
                np.zeros((2, 3)),
                np.zeros(2, int),
                np.ones(2),
                np.tile([0.0, 0, 1], (2, 1)),
                face_ids=np.zeros(2, int),
            )
        self.assertEqual(scene.work["rays"], 0)
        source["surface_model"] = "closed-solid"
        with self.assertRaises(ValueError):
            SheetMeasure(source, scene, [0, 0, 1], [[0, 0, 1]])


if __name__ == "__main__":
    unittest.main()
