"""One-view selection and reconstruction sharing the selected group bytes.

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
    retained = selection.run(selection_args, retain_angle=args.angle)
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
    )
    reconstruction.run(reconstruction_args, retained_groups=retained)


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
    parser.add_argument("--angle", type=int, required=True)
    run(parser.parse_args())
