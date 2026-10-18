"""Prepare a new explicit detour input from the verified current pier geometry."""

import argparse
import copy
import json
import resource
from pathlib import Path

import shapely
from camera_height_inputs import digest
from stair_axis_detours import witness
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_terrain_experiment import frozen


def current_recipe(template, piers):
    declarations = template.get("source_axis_detours")
    keys = template.get("source_ids")
    if (
        not isinstance(declarations, dict)
        or not 1 <= len(declarations) <= 8
        or not isinstance(keys, list)
        or not set(declarations) <= set(keys)
        or not 1 <= len(piers) <= 128
    ):
        raise ValueError("bounded explicit source/pier selections required")
    result, changes = copy.deepcopy(template), []
    for key, row in result["source_axis_detours"].items():
        index = row.get("pier_index")
        if type(index) is not int or not 0 <= index < len(piers):
            raise ValueError("selected current pier is unavailable")
        sha, footprint = witness(piers[index])
        if footprint.geom_type != "Polygon" or not footprint.is_valid:
            raise ValueError("one valid actual current pier footprint required")
        changes.append(
            {
                "source_id": key,
                "pier_index": index,
                "template_pier_xyz_sha256": row.get("pier_xyz_sha256"),
                "current_pier_xyz_sha256": sha,
            }
        )
        row.update(
            pier_xyz_sha256=sha,
            pier_footprint_wkb=shapely.to_wkb(footprint, hex=True),
        )
    return result, changes


def prepare(args):
    commit = frozen()
    receipt = args.output.with_suffix(".binding.json")
    if (
        any(
            path.exists()
            or path.is_symlink()
            or not path.resolve().is_relative_to(Path("/tmp"))
            for path in (args.output, receipt)
        )
        or args.template.stat().st_size > 65536
    ):
        raise ValueError("bounded template and fresh explicit private outputs required")
    state = load_chain(args.resident, args.prior_module)
    recipe, changes = current_recipe(json.loads(args.template.read_text()), state[2])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.output, recipe)
    write_json(
        receipt,
        {
            "commit": commit,
            "template_sha256": digest(args.template),
            "resident_sha256": digest(args.resident / "resident.json"),
            "frame": state[4]["frame"],
            "support_mesh_sha256": digest(Path(state[4]["support_mesh"])),
            "parent_reports": state[4]["parent_reports"],
            "recipe_sha256": digest(args.output),
            "changes": changes,
            "scope": "New input bound to verified current pier bodies. Historical "
            "template untouched; no detour, module or world admission.",
        },
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "template", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    prepare(parser.parse_args())
