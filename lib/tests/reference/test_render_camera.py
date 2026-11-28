import tempfile
import unittest
from datetime import UTC, datetime
from pathlib import Path
from unittest.mock import patch

from outshine import Camera, render_png
from outshine.rendering.image import blender_run


class RenderCameraTest(unittest.TestCase):
    def test_geographic_camera_does_not_require_catalog_pins(self):
        camera = Camera(-34.6037, -58.3816, 72, height_m=75, above_ground=False)
        clock = datetime(2026, 1, 9, 18, tzinfo=UTC)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "view.png"
            with patch(
                "outshine.rendering.image.render_scene", return_value=output
            ) as render:
                self.assertEqual(
                    render_png(camera, output, backend="glsl", time=clock), output
                )
        place, config, destination = render.call_args.args
        self.assertEqual(place["latitude"], -34.6037)
        self.assertEqual(place["longitude"], -58.3816)
        self.assertEqual(place["absolute_eye_m"], 75)
        self.assertEqual(config["utc"], clock.isoformat())
        self.assertEqual(config["source_snapshot"], "latest")
        self.assertNotIn("source_pins", config)
        self.assertEqual(render.call_args.kwargs["backend"], "glsl")
        self.assertEqual(destination, output)

    def test_geographic_camera_passes_atlas_to_both_reference_backends(self):
        camera = Camera(-2.44263704, 34.82496059, 65)
        for backend in ("glsl", "blender"):
            with (
                self.subTest(backend=backend),
                patch("outshine.rendering.image.render_scene") as render,
            ):
                render_png(
                    camera, "view.png", backend=backend, vegetation_atlas=Path("atlas")
                )
                self.assertEqual(
                    render.call_args.args[1]["vegetation_atlas"],
                    str(Path("atlas").resolve()),
                )

    def test_blender_launch_selects_the_platform_gpu_backend(self):
        for platform, backend in (
            ("darwin", "metal"),
            ("linux", "vulkan"),
            ("win32", "vulkan"),
        ):
            with (
                self.subTest(platform=platform),
                patch("outshine.rendering.image.sys.platform", platform),
                patch("outshine.rendering.image.subprocess.run") as run,
            ):
                blender_run("pass", [], binary="blender")
                command = run.call_args.args[0]
                self.assertEqual(command[:3], ["blender", "--gpu-backend", backend])
