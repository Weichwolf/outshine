"""Single-image entry for the Outshine Python reference."""

import argparse
from datetime import datetime
from pathlib import Path

from .rendering.image import Camera, render_png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--latitude", type=float, required=True)
    parser.add_argument("--longitude", type=float, required=True)
    parser.add_argument(
        "--heading", type=float, required=True, help="degrees clockwise from north"
    )
    parser.add_argument("--pitch", type=float, default=0)
    parser.add_argument(
        "--height", type=float, default=1.7, help="metres above the surface"
    )
    parser.add_argument(
        "--absolute-height",
        action="store_true",
        help="interpret height above sea level",
    )
    parser.add_argument(
        "--fov", type=float, default=54, help="vertical field of view in degrees"
    )
    parser.add_argument(
        "--time", type=datetime.fromisoformat, help="ISO 8601 with timezone"
    )
    parser.add_argument(
        "--weather",
        choices=("clear", "haze", "broken-cloud", "overcast"),
        default="clear",
    )
    parser.add_argument("--backend", choices=("blender", "glsl"), default="blender")
    parser.add_argument("--engine", choices=("eevee", "cycles"), default="eevee")
    parser.add_argument("--blender", type=Path, help="Blender executable")
    parser.add_argument("--cache", type=Path, default=Path("build/reference-sources"))
    parser.add_argument(
        "--snapshot", default="latest", help="OpenFreeMap snapshot or 'latest'"
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        camera = Camera(
            args.latitude,
            args.longitude,
            args.heading,
            args.pitch,
            args.height,
            args.fov,
            above_ground=not args.absolute_height,
        )
        render_png(
            camera,
            args.output,
            backend=args.backend,
            time=args.time,
            weather=args.weather,
            cache=args.cache,
            snapshot=args.snapshot,
            engine=args.engine,
            blender=args.blender,
        )
    except (ValueError, FileNotFoundError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
