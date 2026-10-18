"""Export the versioned cold procedural crown without private temporary inputs."""

import argparse
import hashlib
import json
import sys
import tempfile
from dataclasses import asdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vegetation"))
from botanical_profiles import plant_recipe
from limb_paths import dominant_paths
from native_plant_limb_reference import encode
from oak_scaffold import tapered_radii
from oak_skin_experiment import seed_continuations
from spatial_group_source import load_source


def export(inputs, output):
    manifest = json.loads((inputs / "manifest.json").read_text())
    for name, expected in manifest["files_sha256"].items():
        if hashlib.sha256((inputs / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"changed original source: {name}")
    metadata = json.loads((inputs / "comparison.json").read_text())
    source = load_source(inputs)
    crown = source.crown
    header = [
        len(crown.parents),
        len(crown.variants),
        crown.seed,
        crown.length_m,
        crown.bend_rad,
        crown.petiole_m,
        crown.foliage_depth_m,
        1.12,
        0.3,
    ]
    rows = [" ".join(map(str, header))]
    rows += [
        f"{parent} " + " ".join(format(x, ".17g") for x in point)
        for parent, point in zip(crown.parents, crown.positions, strict=True)
    ]
    rows.append(" ".join(map(str, crown.variants)))
    (output / "crown.input").write_text("\n".join(rows) + "\n")
    recipe = metadata["recipe"]
    radii = tapered_radii(
        crown.parents, recipe["root_radius_m"], recipe["terminal_radius_m"]
    )
    seed_count = metadata["products"]["spatial"]["source_seed_nodes"]
    hints = seed_continuations(crown.parents, crown.parents[:seed_count])
    paths = dominant_paths(
        crown.positions, crown.parents, radii, max_nodes=16384, continuations=hints
    )
    if paths is None or max(map(len, paths)) > 65:
        raise ValueError("complete original wood exceeds the declared limb capacity")
    (output / "wood.input").write_text(encode(crown.positions, radii, paths, 2, 8))
    blades = plant_recipe("quercus", crown.seed).recipe()
    blades["blades"] = [asdict(b) for b in source.blades]
    (output / "blades.json").write_text(json.dumps(blades) + "\n")
    receipt = {
        "source_manifest": manifest,
        "source_catalog": source.identity,
        "nodes": len(crown.parents),
        "limbs": len(paths),
        "leaves": len(crown.variants),
        "files_sha256": {
            name: hashlib.sha256((output / name).read_bytes()).hexdigest()
            for name in ("crown.input", "wood.input", "blades.json")
        },
    }
    (output / "source.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        raise ValueError("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    print(json.dumps(export(args.inputs.resolve(), output)), flush=True)


if __name__ == "__main__":
    main()
