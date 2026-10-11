"""Derive complete camera and per-light meshes from a finished spherical view cache."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from cloud_product import view_snapshot
from preview_lighting import FILL_POSITION, SUN_ROTATION
from scipy.spatial.transform import Rotation


def add_shadow_products(directory):
    manifest = json.loads((directory / "cloud-product.json").read_text())
    recipe = json.loads((directory / "views-tree.json").read_text())
    assert manifest["plan_identity"] == recipe["plan_identity"]
    assert manifest["version"] == "spherical-group-views-1"
    with np.load(directory / manifest["cache_mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    for value in mesh.values():
        value.setflags(write=False)
    records = json.loads((directory / "cloud-groups.json").read_text())
    recipe["products"][-1]["pass_role"] = "camera"
    views = {
        "sun": {
            "view_direction": Rotation.from_euler("xyz", SUN_ROTATION).apply((0, 0, 1))
        },
        "fill": {"camera": FILL_POSITION},
    }
    selections = {}
    for name, query in views.items():
        selected, indices = view_snapshot(mesh, records, **query)
        assert len(selected["faces"]) == 1260 and len(np.unique(indices)) == 630
        filename = f"shadow-{name}-mesh.npz"
        np.savez_compressed(directory / filename, **selected)
        recipe["products"].append(
            manifest["product"]
            | {
                "mesh": filename,
                "triangles_per_instance": len(selected["faces"]),
                "pass_role": "shadow",
                "shadow_light": name,
            }
        )
        selections[name] = indices.tolist()
    recipe["name"] = "views-shadow-pass"
    recipe["source_commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    recipe["source_modified"] = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    recipe["shadow_selections"] = selections
    (directory / "views-shadow-pass.json").write_text(
        json.dumps(recipe, indent=2) + "\n"
    )
    return {
        "groups_per_light": len(records),
        "shadow_triangles_per_light": 1260,
        "lights": list(views),
        "plan_identity": manifest["plan_identity"],
        "source_commit": recipe["source_commit"],
        "source_modified": recipe["source_modified"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path("build/experiments/vegetation/selection")
    )
    args = parser.parse_args()
    print(json.dumps(add_shadow_products(args.directory), indent=2))


if __name__ == "__main__":
    main()
