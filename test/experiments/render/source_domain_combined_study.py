"""Select whole-source demand, then filter budgets before reading domain payload.

Finite Python experiment only; no World cache, device or native raster admission.
"""

import argparse
from pathlib import Path
from types import SimpleNamespace

import source_domain_prepared_view as selection
import source_domain_reconstruct_study as reconstruction


def run(args):
    selection_args = SimpleNamespace(
        bounds=args.bounds,
        bounds_sha256=args.bounds_sha256,
        chunk=args.chunk,
        fallback=args.fallback,
        output=args.view_output,
        angle=args.angle,
    )
    _, selected_sources = selection.run(
        selection_args, retain_angle=args.angle, metadata_only=True
    )
    reconstruction_args = SimpleNamespace(
        view=args.view_output,
        bounds=args.bounds,
        binding=args.binding,
        recipes=args.recipes,
        recipes_index_sha256=args.recipes_index_sha256,
        span_index=args.span_index,
        span_index_sha256=args.span_index_sha256,
        chunk=args.chunk,
        fallback=args.fallback,
        angle=args.angle,
        output=args.output,
        **{
            name: getattr(args, name, None)
            for name in (
                "max_controls",
                "max_triangles",
                "original_ranges",
                "control_index",
                "control_index_sha256",
            )
        },
    )
    reconstruction.run(reconstruction_args, selected_sources=selected_sources)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "bounds",
        "binding",
        "recipes",
        "chunk",
        "fallback",
        "view-output",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("bounds-sha256", "recipes-index-sha256"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--span-index", type=Path, required=True)
    parser.add_argument("--span-index-sha256", required=True)
    parser.add_argument("--max-controls", type=int)
    parser.add_argument("--max-triangles", type=int)
    parser.add_argument("--original-ranges", type=Path)
    parser.add_argument("--control-index", type=Path)
    parser.add_argument("--control-index-sha256")
    parser.add_argument("--angle", type=int, required=True)
    run(parser.parse_args())
