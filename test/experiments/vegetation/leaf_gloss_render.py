"""Current six source-loaded leaves under declared area/additive gloss alternatives."""

import argparse
from pathlib import Path

from leaf_gloss import LeafGloss
from sheet_leaf_render import run

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("area", "additive"), required=True)
    parser.add_argument("--mr-fraction", type=float, default=0.12)
    parser.add_argument("--roughness", type=float, default=0.3)
    args = parser.parse_args()
    run(
        args.inputs,
        args.output,
        2,
        closure=LeafGloss(args.roughness, args.mr_fraction, args.mode),
    )
