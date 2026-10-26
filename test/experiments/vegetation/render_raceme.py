"""Same cached near camera/light as the earlier study, with a closed-bud material."""

import argparse
import hashlib
import json
from pathlib import Path

from blender_preview import material
from render_understory import palette, render


def phase_palette(inputs):
    mats = palette(inputs)
    mats.append(material("young ivory-green bud", (0.34, 0.45, 0.18), 0.52))
    return mats


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output, "plant", palette_factory=phase_palette)
    path = args.output / "plant.json"
    receipt = json.loads(path.read_text())
    state = args.inputs / "maturation-state.json"
    receipt["phenology"] = json.loads(state.read_text())
    receipt["input_hashes"][state.name] = hashlib.sha256(state.read_bytes()).hexdigest()
    path.write_text(json.dumps(receipt, indent=2) + "\n")
