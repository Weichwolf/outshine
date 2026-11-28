"""Render the pinned comparison cameras used by the Python tests."""

import argparse
from pathlib import Path

from outshine.rendering.image import blender_run, gl_context, render_scene

from .fixtures.vegetation.catalog import load_sites, site_views
from .fixtures.vienna.catalog import ROOT, available_views, load_catalog, selected_views


def main():
    config = load_catalog()
    sites = load_sites()
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--place", action="append", choices=[v["id"] for v in available_views(config)]
    )
    selection.add_argument(
        "--vegetation-site",
        action="append",
        choices=sites,
        help="render the pinned views for a vegetation corpus site",
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
    if args.vegetation_site:
        views = [
            entry
            for site_id in dict.fromkeys(args.vegetation_site)
            for entry in site_views(sites[site_id], config)
        ]
    else:
        views = [(view, config) for view in selected_views(config, args.place)]
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
            for view, view_config in views:
                if args.export_sky:
                    export_sky(ctx, view, view_config, output)
                else:
                    render(
                        ctx,
                        view,
                        view_config,
                        output / (view["id"] + ".npy"),
                        sky_dir=args.sky_dir,
                        linear=True,
                        street_sky=args.street_sky,
                    )
        return
    for view, view_config in views:
        render_scene(
            view,
            view_config,
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
