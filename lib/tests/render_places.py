"""Render the pinned comparison cameras used by the Python tests."""

import argparse
from pathlib import Path

from outshine.rendering.image import blender_run, gl_context, render_scene

from .fixtures.vienna.catalog import ROOT, available_views, load_catalog, selected_views


def main():
    config = load_catalog()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--place", action="append", choices=[v["id"] for v in available_views(config)]
    )
    parser.add_argument("--backend", choices=("blender", "glsl"), default="blender")
    parser.add_argument("--blender", type=Path)
    parser.add_argument("--engine", choices=("eevee", "cycles"), default="eevee")
    parser.add_argument("--cycles-samples", type=int)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--sky-dir", type=Path)
    parser.add_argument("--export-sky", action="store_true")
    parser.add_argument("--linear", action="store_true")
    parser.add_argument("--display-linear", type=Path, nargs="+")
    parser.add_argument("--street-bounce", action="store_true")
    parser.add_argument("--street-sky", action="store_true")
    args = parser.parse_args()
    default = (
        "glsl-agx"
        if args.display_linear
        else "glsl"
        if args.backend == "glsl"
        else "places"
    )
    output = args.output or ROOT / "build/shots" / default
    if args.display_linear:
        request = [list(map(str, args.display_linear)), str(output)]
        blender_run(
            "from outshine.rendering.blender import display_request; display_request()",
            request,
            args.blender,
        )
        return
    if args.export_sky or args.linear:
        from outshine.rendering.glsl import export_sky, render

        with gl_context() as ctx:
            for view in selected_views(config, args.place):
                if args.export_sky:
                    export_sky(ctx, view, config, output)
                else:
                    render(
                        ctx,
                        view,
                        config,
                        output / (view["id"] + ".npy"),
                        sky_dir=args.sky_dir,
                        linear=True,
                        street_sky=args.street_sky,
                    )
        return
    for view in selected_views(config, args.place):
        render_scene(
            view,
            config,
            output / (view["id"] + ".png"),
            backend=args.backend,
            engine=args.engine,
            blender=args.blender,
            sky_dir=args.sky_dir,
            street_bounce=args.street_bounce,
            street_sky=args.street_sky,
            cycles_samples=args.cycles_samples,
        )


if __name__ == "__main__":
    main()
