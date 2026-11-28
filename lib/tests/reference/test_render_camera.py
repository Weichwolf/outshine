import tempfile
import unittest
from datetime import UTC, datetime
from pathlib import Path
from unittest.mock import patch

from outshine import Camera, render_png


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
