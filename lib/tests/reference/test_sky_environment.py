import tempfile
import unittest
from pathlib import Path

import numpy as np
from outshine.rendering.sky_environment import (
    CLOUD_ENVIRONMENT_SIZE,
    SKY_ENVIRONMENT_SIZE,
    diffuse_coefficients,
    load_environment,
    sample_radiance,
    save_environment,
)
from outshine.weather.air_profile import (
    AEROSOL_HEIGHT_KM,
    aerosol_extinction,
    direct_sun_transmission,
)
from outshine.weather.cloud_profile import SUN_MAP_SIZE, WIND_KM_S, cloud_profile


class SkyEnvironmentTest(unittest.TestCase):
    def test_sky_lookup_wraps_longitude_and_clamps_both_poles(self):
        width, height = SKY_ENVIRONMENT_SIZE
        longitude = np.pi * (1 - 2 * (np.arange(width) + 0.5) / width)
        latitude = np.pi * ((np.arange(height) + 0.5) / height - 0.5)
        x = np.cos(latitude)[:, None] * np.cos(longitude)
        y = np.cos(latitude)[:, None] * np.sin(longitude)
        z = np.broadcast_to(np.sin(latitude)[:, None], (height, width))
        radiance = 0.3 + 0.2 * np.stack((x, y, z), axis=-1)
        directions = np.array(
            [(1, 0, 0), (0, 1, 0), (-1, 1e-8, 0), (-1, -1e-8, 0), (0, 0, 1), (0, 0, -1)]
        )
        np.testing.assert_allclose(
            sample_radiance(radiance, directions), 0.3 + 0.2 * directions, atol=0.0013
        )

    def test_diffuse_kernel_preserves_uniform_light_and_orients_the_sky(self):
        width, height = SKY_ENVIRONMENT_SIZE
        latitude = np.pi * ((np.arange(height) + 0.5) / height - 0.5)
        base = np.array([0.22, 0.32, 0.45])
        gradient = np.array([0.11, 0.08, 0.05])
        radiance = base + np.sin(latitude)[:, None, None] * gradient
        radiance = np.broadcast_to(radiance, (height, width, 3))

        coefficients = diffuse_coefficients(radiance)
        np.testing.assert_allclose(0.2820947918 * coefficients[0], base, atol=1e-6)
        np.testing.assert_allclose(
            0.4886025119 * coefficients[2], (2 / 3) * gradient, atol=2e-6
        )
        np.testing.assert_allclose(coefficients[[1, 3, 4, 5, 7, 8]], 0, atol=1e-6)

    def test_environment_is_tied_to_the_rendered_altitude_and_sun(self):
        radiance = np.full((*SKY_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        sun = (0.0, 0.6, 0.8)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            save_environment(path, radiance, sun, 156.25)
            np.testing.assert_array_equal(
                load_environment(path, sun, 156.25).radiance, radiance
            )
            with self.assertRaisesRegex(ValueError, "sun or eye altitude"):
                load_environment(path, (0.6, 0.0, 0.8), 156.25)
            with self.assertRaisesRegex(ValueError, "sun or eye altitude"):
                load_environment(path, sun, 157.25)

    def test_legacy_clear_and_current_hazy_archives_are_distinct(self):
        radiance = np.full((*SKY_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        sun, eye = (0.0, 0.6, 0.8), 477.25
        haze = aerosol_extinction("haze")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            np.savez(path, radiance=radiance, sun_enu=sun, eye_m=eye)
            np.testing.assert_array_equal(
                load_environment(path, sun, eye).radiance, radiance
            )
            with self.assertRaisesRegex(ValueError, "aerosol extinction"):
                load_environment(path, sun, eye, aerosol=haze)
            save_environment(path, radiance, sun, eye, aerosol=haze)
            np.testing.assert_array_equal(
                load_environment(path, sun, eye, aerosol=haze).radiance, radiance
            )
            with self.assertRaisesRegex(ValueError, "aerosol extinction"):
                load_environment(path, sun, eye)

    def test_solar_transmission_matches_vertical_column_and_varies_with_elevation(self):
        haze = aerosol_extinction("haze")
        clear = np.array(direct_sun_transmission(0.0, 1.0, 0.0))
        rayleigh = 8 * (1 - np.exp(-100 / 8)) * np.array([0.005802, 0.013558, 0.033100])
        ozone = 15 * np.array([0.000650, 0.001881, 0.000085])
        np.testing.assert_allclose(clear, np.exp(-rayleigh - ozone), atol=1e-8)
        zenith = direct_sun_transmission(0.0, 1.0, haze)
        np.testing.assert_allclose(zenith / clear, np.exp(-haze * AEROSOL_HEIGHT_KM))
        low = direct_sun_transmission(0.0, 0.2, haze)
        self.assertTrue(np.all(np.array(low) < zenith))
        self.assertGreater(low[0] / low[2], zenith[0] / zenith[2])
        self.assertTrue(
            np.all(np.array(direct_sun_transmission(1500.0, 1.0, haze)) > zenith)
        )
        self.assertEqual(direct_sun_transmission(0.0, -0.01, 0.0), (0.0, 0.0, 0.0))
        self.assertGreater(direct_sun_transmission(1500.0, -0.01, 0.0)[0], 0.0)
        self.assertEqual(direct_sun_transmission(110_000.0, 1.0, 0.0), (1.0, 1.0, 1.0))

    def test_solar_transmission_matches_the_atmosphere_shader(self):
        try:
            from outshine.rendering import glsl
            from outshine.rendering.image import gl_context

            context = gl_context()
        except Exception as error:  # noqa: BLE001
            self.skipTest(f"OpenGL reference unavailable: {error}")
        with context as ctx:
            fragment = (
                "#version 410 core\n"
                + glsl.shader(glsl.ROOT / "src/render/shaders/medium.glsl")
                + glsl.shader(glsl.HERE / "clear_air.glsl")
                + """
out vec4 colour;
uniform float eyeKm;
uniform float sunVertical;
uniform float aerosol;
void main() {
  Medium air = referenceAir(aerosol);
  vec3 transmission = mediumTransmittanceToTop(air, MediumLook(eyeKm, sunVertical), 2048u);
  colour = vec4(transmission, 1.);
}
"""
            )
            program = ctx.program(vertex_shader=glsl.SCREEN, fragment_shader=fragment)
            quad = ctx.vertex_array(program, [])
            target = ctx.simple_framebuffer((1, 1), components=4, dtype="f4")
            target.use()
            for eye_m, elevation, haze in (
                (170, 61, 0.0),
                (170, 24, 0.0),
                (170, 8, 0.04),
                (170, 0, 0.04),
                (1500, -0.5, 0.04),
            ):
                with self.subTest(eye_m=eye_m, elevation=elevation, haze=haze):
                    vertical = np.sin(np.radians(elevation))
                    program["eyeKm"].value = 6360 + eye_m * 0.001
                    program["sunVertical"].value = vertical
                    program["aerosol"].value = haze
                    quad.render(vertices=3)
                    gpu = np.frombuffer(
                        target.read(components=3, dtype="f4"), dtype="f4"
                    )
                    np.testing.assert_allclose(
                        direct_sun_transmission(eye_m, vertical, haze),
                        gpu,
                        atol=2e-5,
                        rtol=0,
                    )

    def test_cloud_sky_keeps_its_advected_field_and_direct_light(self):
        def profile(name, elapsed):
            return cloud_profile(name, 16.22, 48.24, (0, 0), elapsed, 2083)

        broken = profile("broken-cloud", 0)
        later = profile("broken-cloud", 90)
        np.testing.assert_allclose(
            np.array(later.origin_km) - broken.origin_km, -90 * WIND_KM_S
        )
        np.testing.assert_array_equal(
            broken.sun_map_origin([5, 10, 2], 300), [5, 10, 2]
        )
        np.testing.assert_array_equal(
            broken.sun_map_origin([5, 10, 2900], 300), [5, 10, 900]
        )
        self.assertIsNone(profile("clear", 0))
        sun, eye = (0.0, 0.6, 0.8), 477.25
        radiance = np.full((*CLOUD_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            save_environment(path, radiance, sun, eye, clouds=broken, cloud_sun=0.37)
            restored = load_environment(path, sun, eye, clouds=broken)
            self.assertEqual(restored.cloud_sun, 0.37)
            self.assertIsNone(restored.cloud_sun_map)
            np.testing.assert_array_equal(restored.radiance, radiance)
            for mismatch in (None, later, profile("overcast", 0)):
                with self.assertRaisesRegex(ValueError, "cloud recipe"):
                    load_environment(path, sun, eye, clouds=mismatch)
            save_environment(path, radiance, sun, eye, clouds=broken, cloud_sun=np.nan)
            with self.assertRaisesRegex(ValueError, "solar transmission"):
                load_environment(path, sun, eye, clouds=broken)

    def test_cloud_sun_map_round_trips_and_checks_transmission(self):
        clouds = cloud_profile("broken-cloud", 16.22, 48.24, (0, 0), 0, 2083)
        sun, eye = (0.0, 0.6, 0.8), 477.25
        radiance = np.full((*CLOUD_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        transmission = np.full((SUN_MAP_SIZE, SUN_MAP_SIZE), 0.8, dtype="f4")
        transmission[: SUN_MAP_SIZE // 2] = 0.2
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            save_environment(
                path, radiance, sun, eye, clouds=clouds, cloud_sun_map=transmission
            )
            restored = load_environment(path, sun, eye, clouds=clouds)
            np.testing.assert_array_equal(restored.cloud_sun_map, transmission)

            transmission[0, 0] = 1.2
            save_environment(
                path, radiance, sun, eye, clouds=clouds, cloud_sun_map=transmission
            )
            with self.assertRaisesRegex(ValueError, "solar transmission map"):
                load_environment(path, sun, eye, clouds=clouds)

    def test_first_revision_cloud_radiance_replays_at_the_original_field(self):
        sun, eye = (0.0, 0.6, 0.8), 477.25
        clouds = cloud_profile("broken-cloud", 16.22, 48.24, (0, 0), 0, 2083)
        old_key = (1, *clouds.archive_key[1:])
        radiance = np.full((*CLOUD_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            for key in (old_key, (0, *old_key[1:]), (*old_key[:-1], old_key[-1] + 1)):
                np.savez(
                    path,
                    radiance=radiance,
                    sun_enu=sun,
                    eye_m=eye,
                    clouds=key,
                    cloud_sun=0.37,
                )
                if key == old_key:
                    restored = load_environment(path, sun, eye, clouds=clouds)
                    np.testing.assert_array_equal(restored.radiance, radiance)
                    self.assertEqual(restored.cloud_sun, 0.37)
                else:
                    with self.assertRaisesRegex(ValueError, "cloud recipe"):
                        load_environment(path, sun, eye, clouds=clouds)


if __name__ == "__main__":
    unittest.main()
