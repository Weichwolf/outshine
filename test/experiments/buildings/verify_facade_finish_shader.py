"""Physical mesh rays compare finish BSDF weights to independent metric fields."""

import unittest

import drjit as dr
import mitsuba as mi
import numpy as np
import shapely
from mitsuba_facade import bind, material, validate
from mitsuba_mesh import mesh as physical_mesh
from outshine_ref.buildings.facade_finish import FacadeFinish
from outshine_ref.buildings.facades import FacadeRepeat
from provider_contract import attributes
from tile_buildings import prism
from tile_facades import project


class FacadeFinishShaderControls(unittest.TestCase):
    def test_actual_front_oblique_and_far_rays_match_metric_area_weights(self):
        mi.set_variant("llvm_ad_rgb")
        dr.set_thread_count(2)
        bind()
        row = {
            "id": "finish-wall",
            "polygon": shapely.box(0, 0, 20, 10),
            "properties": {"render_height": 15},
        }
        parameters = attributes(row["properties"])
        original, _ = prism(row, parameters, 12)
        data, selected, _ = project(original, row, parameters, (0.48, 0.53), 3.2, 3.0)
        data["materials"][selected] = 32768
        snapshot = {key: value.copy() for key, value in data.items()}
        spec = {"fractions": [0.48, 0.53], "wall_material": 12}
        validate(data, {"32768": spec})
        repeat = FacadeRepeat(20.0, 15.0, 6, 5, 0.48, 0.53)
        finish = FacadeFinish()
        wall = {
            "type": "principled",
            "base_color": {"type": "rgb", "value": [0.7, 0.6, 0.5]},
            "roughness": 0.7,
            "metallic": 0.0,
        }
        points = np.array(
            [
                (x, 0.0, z)
                for x in (0.89, 1.66, 4.22, 5.0, 8.333, 10.0, 17.5, 19.0)
                for z in (0.05, 1.5, 3.0, 4.5, 7.5, 9.0, 13.5)
            ]
        )
        for eye in ([10, -30, 8], [10, -1500, 8], [-80, -5, 12]):
            config = {
                "eye": eye,
                "target": [10, 0, 7.5],
                "profile": [1920, 1080],
                "fov_deg": 48,
                "facade_finishes": {"frame_m": finish.frame_m, "band_m": finish.band_m},
            }
            bsdf, _ = material(spec, config, wall)
            shape, _ = physical_mesh("wall", data, np.arange(len(data["faces"])), bsdf)
            scene = mi.load_dict({"type": "scene", "wall": shape})
            ray = mi.Ray3f(mi.Point3f(eye), mi.Vector3f((points - np.asarray(eye)).T))
            si = scene.ray_intersect(ray)
            self.assertTrue(bool(dr.all(si.is_valid())))
            outer = mi.load_dict(bsdf["weight"])
            opening = np.asarray(outer.eval_1(si))
            glass = (
                np.asarray(mi.load_dict(bsdf["bsdf_1"]["weight"]).eval_1(si)) * opening
            )
            band = np.asarray(mi.load_dict(bsdf["bsdf_0"]["weight"]).eval_1(si)) * (
                1 - opening
            )
            actual = np.column_stack([1 - opening - band, opening - glass, glass, band])
            scale = np.asarray(repeat.cell_size_m)
            centres = np.asarray(si.uv).T * scale
            footprint = np.asarray(outer.footprint(si)).T * scale
            expected = finish.coverage(repeat, centres, footprint)
            np.testing.assert_allclose(actual, expected, atol=2e-5, rtol=2e-5)
            np.testing.assert_allclose(actual.sum(axis=1), 1.0, atol=2e-7)
        for key in data:
            np.testing.assert_array_equal(data[key], snapshot[key])

    def test_direct_opening_patches_retain_the_original_surface_path(self):
        mi.set_variant("llvm_ad_rgb")
        bind()
        wall = {
            "type": "principled",
            "base_color": {"type": "rgb", "value": [0.7, 0.6, 0.5]},
        }
        config = {
            "eye": [0, -10, 5],
            "target": [0, 0, 5],
            "profile": [1920, 1080],
            "fov_deg": 48,
            "facade_finishes": {"frame_m": 0.06, "band_m": 0.16},
        }
        result, _ = material({"fractions": [1.0, 1.0]}, config, wall)
        self.assertEqual(result["weight"]["type"], "outshine_facade_coverage")


if __name__ == "__main__":
    unittest.main()
