"""Independent whole cached-source, per-limb topology and unchanged-stem checks."""

import argparse
import hashlib
import json
from itertools import pairwise
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_union_replay import topology
from limb_experiment import canonical_wood
from limb_surface import path_recipe
from oak_scaffold import tapered_radii
from oak_scaffold_replay import check_wood_binding, replay
from oak_skin_experiment import replace_wood


def continuous_binding(inputs, name, metadata, crown, wood):
    entry = json.loads((inputs / name / "entry.json").read_text())
    recipe = entry["product_recipe"]
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    assert (
        identity
        == entry["product_identity"]
        == metadata["products"][name]["product_identity"]
    )
    assert (
        recipe["recipe"] == metadata["recipe"]
        and recipe["leaf_plan_identity"] == crown.identity
    )
    before = json.loads((inputs / "recursive/entry.json").read_text())
    assert recipe["source_product_identity"] == before["product_identity"]
    source = inputs / "recursive/wood.npz"
    assert (
        hashlib.sha256(source.read_bytes()).hexdigest() == recipe["source_wood_sha256"]
    )
    with np.load(source) as a:
        original = {k: a[k] for k in a.files}
    hints = np.full(len(crown.parents), -1, np.int32)
    for parent, child in recipe["source_continuations"]:
        hints[parent] = child
    radii = tapered_radii(
        crown.parents,
        metadata["recipe"]["root_radius_m"],
        metadata["recipe"]["terminal_radius_m"],
    )
    expected, paths, counts = replace_wood(crown, original, radii, hints)
    for key, value in expected.items():
        np.testing.assert_array_equal(value, wood[key])
    del expected
    edges = np.array([e for path in paths for e in pairwise(path)])
    np.testing.assert_array_equal(
        np.sort(edges[:, 1]), np.arange(1, len(crown.parents))
    )
    np.testing.assert_array_equal(edges[:, 0], crown.parents[edges[:, 1]])
    geometry = []
    fv = ff = 0
    anchor_error = 0.0
    for path in paths:
        stations = (len(path) - 1) * 2 + 1
        nv = stations * 8 + 2
        nf = stations * 16
        part = {
            "vertices": wood["vertices"][fv : fv + nv],
            "faces": wood["faces"][ff : ff + nf] - fv,
            "normals": wood["normals"][ff * 3 : (ff + nf) * 3],
        }
        checked = topology(part)
        assert checked["euler_characteristic"] == 2
        geometry.append(checked)
        spine, _ = path_recipe(crown.positions, radii, path)
        anchor = spine.evaluate(np.linspace(0, 1, len(path)))
        np.testing.assert_allclose(
            anchor, crown.positions[np.array(path)], atol=2e-14, rtol=0
        )
        anchor_error = max(
            anchor_error, float(abs(anchor - crown.positions[np.array(path)]).max())
        )
        fv += nv
        ff += nf
    assert fv == counts["new_body_vertices"] and ff == counts["new_body_faces"]
    ov, of = counts["old_body_vertices"], counts["old_body_faces"]
    for key, oldcut, newcut in (
        ("vertices", ov, fv),
        ("normals", of * 3, ff * 3),
        ("corner_uv", of * 3, ff * 3),
        ("surface", of, ff),
    ):
        np.testing.assert_array_equal(original[key][oldcut:], wood[key][newcut:])
    np.testing.assert_array_equal(original["faces"][of:] - ov, wood["faces"][ff:] - fv)
    with patch(
        "limb_experiment.limb_mesh", side_effect=AssertionError("curve before capacity")
    ):
        assert (
            canonical_wood(
                crown,
                radii=radii,
                max_nodes=16384,
                continuations=hints,
                max_faces=ff - 1,
            )
            is None
        )
    return {
        "closed_components_checked": len(paths),
        "single_vertex_fans": sum(d["single_vertex_fans"] for d in geometry),
        "maximum_anchor_component_error_m": anchor_error,
        "minimum_limb_volume_m3": min(d["signed_volume_m3"] for d in geometry),
        "unchanged_stem_payload_fields": [
            "vertices",
            "faces relative indices",
            "normals",
            "corner_uv",
            "surface",
        ],
        "scope": "each dominant limb closed with one vertex fan, genus0/positive volume; side-fork and short-shoot solids remain separate; no whole union/self-intersection proof",
    }


def run(inputs, output):
    extra = {}

    def check(folder, name, metadata, crown, wood):
        if name == "recursive":
            check_wood_binding(folder, name, metadata, crown, wood)
        else:
            extra[name] = continuous_binding(folder, name, metadata, crown, wood)

    replay(inputs, output, wood_check=check)
    result = json.loads(output.read_text())
    result["continuous_wood"] = extra
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
