"""Render one camera to one PNG with Blender or the GLSL reference backend."""

import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import UTC, datetime
from math import isfinite
from pathlib import Path
from tempfile import TemporaryDirectory


@dataclass(frozen=True, slots=True)
class Camera:
    latitude: float
    longitude: float
    heading_deg: float
    pitch_deg: float = 0.0
    height_m: float = 1.7
    vertical_fov_deg: float = 54.0
    above_ground: bool = True

    def __post_init__(self):
        if (
            not all(
                map(
                    isfinite,
                    (
                        self.latitude,
                        self.longitude,
                        self.heading_deg,
                        self.pitch_deg,
                        self.height_m,
                        self.vertical_fov_deg,
                    ),
                )
            )
            or abs(self.latitude) >= 85.05112878
            or abs(self.longitude) > 180
            or abs(self.pitch_deg) >= 90
            or not 0 < self.vertical_fov_deg < 180
        ):
            raise ValueError(
                "finite camera pose in the Web Mercator source domain required"
            )


def render_png(
    camera,
    output,
    *,
    backend="blender",
    time=None,
    weather="clear",
    cache="build/reference-sources",
    snapshot="latest",
    blender=None,
    engine="eevee",
):
    """Render a geographic view. Camera height is above solved ground by default."""
    time = datetime.now(UTC) if time is None else time
    if time.utcoffset() is None:
        raise ValueError("an explicit timezone is required")
    place = {
        "id": "camera",
        "latitude": camera.latitude,
        "longitude": camera.longitude,
        "heading_deg": camera.heading_deg,
        "pitch_deg": camera.pitch_deg,
        "vertical_fov_deg": camera.vertical_fov_deg,
        "weather": weather,
    }
    if not camera.above_ground:
        place["absolute_eye_m"] = camera.height_m
    config = {
        "utc": time.isoformat(),
        "eye_height_m": camera.height_m,
        "vertical_fov_deg": camera.vertical_fov_deg,
        "seed": 2083,
        "source_snapshot": snapshot,
        "source_cache": str(Path(cache).resolve()),
    }
    return render_scene(
        place, config, output, backend=backend, engine=engine, blender=blender
    )


def gl_context():
    import moderngl

    options = (
        {"backend": "egl"}
        if sys.platform.startswith("linux") and not os.environ.get("DISPLAY")
        else {}
    )
    return moderngl.create_standalone_context(require=410, **options)


def blender_run(expression, request, binary=None):
    binary = binary or shutil.which("blender")
    if binary is None and sys.platform == "darwin":
        app = Path("/Applications/Blender.app/Contents/MacOS/Blender")
        if app.is_file():
            binary = app
    if binary is None:
        raise FileNotFoundError("Blender is required. Set --blender or put it on PATH.")
    gpu_backend = "metal" if sys.platform == "darwin" else "vulkan"
    python_paths = dict.fromkeys(
        [
            str(Path(__file__).resolve().parents[2]),
            *(entry for entry in sys.path if Path(entry).name == "site-packages"),
        ]
    )
    with TemporaryDirectory(prefix="outshine-blender-") as directory:
        payload = Path(directory) / "request.json"
        payload.write_text(json.dumps(request))
        subprocess.run(
            [
                str(binary),
                "--gpu-backend",
                gpu_backend,
                "-b",
                "--factory-startup",
                "--python-use-system-env",
                "--python-exit-code",
                "1",
                "--python-expr",
                expression,
                "--",
                str(payload),
            ],
            env=os.environ | {"PYTHONPATH": os.pathsep.join(python_paths)},
            check=True,
        )


def render_scene(
    place,
    config,
    output,
    *,
    backend="blender",
    engine="eevee",
    blender=None,
    sky_dir=None,
    street_bounce=False,
    street_sky=False,
    cycles_samples=None,
):
    output = Path(output).resolve()
    if output.suffix.lower() != ".png":
        raise ValueError("output must be a PNG path")
    if backend not in ("blender", "glsl"):
        raise ValueError(f"unknown renderer {backend!r}")
    weather = place.get("weather", config.get("weather", "clear"))
    if sky_dir is None and weather in ("broken-cloud", "overcast"):
        from .glsl import export_sky

        with TemporaryDirectory(prefix="outshine-reference-sky-") as directory:
            with gl_context() as ctx:
                export_sky(ctx, place, config, Path(directory))
            return render_scene(
                place,
                config,
                output,
                backend=backend,
                engine=engine,
                blender=blender,
                sky_dir=directory,
                street_bounce=street_bounce,
                street_sky=street_sky,
                cycles_samples=cycles_samples,
            )
    sky_dir = Path(sky_dir).resolve() if sky_dir is not None else None
    if backend == "glsl":
        from .glsl import render

        with gl_context() as ctx:
            render(ctx, place, config, output, sky_dir=sky_dir)
    else:
        request = [
            place,
            config,
            {
                "output": str(output),
                "engine": engine,
                "sky_dir": str(sky_dir) if sky_dir is not None else None,
                "street_bounce": street_bounce,
                "street_sky": street_sky,
                "cycles_samples": cycles_samples,
            },
        ]
        blender_run(
            "from outshine.rendering.blender import render_request; render_request()",
            request,
            blender,
        )
    return output
