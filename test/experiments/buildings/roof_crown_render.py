"""Render one exact research product; exit after verified save, before bpy teardown."""

import argparse
import os
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=16)
    args = parser.parse_args()
    target = args.output.resolve()
    if target == Path("/tmp").resolve() or not target.is_relative_to(
        Path("/tmp").resolve()
    ):
        parser.error("--output requires a subdirectory of /tmp")
    if not 1 <= args.samples <= 4096:
        parser.error("--samples requires1..4096")
    from blender_city import render

    render(args.geometry, target, args.samples)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)


if __name__ == "__main__":
    main()
