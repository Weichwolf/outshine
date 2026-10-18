"""Replace only prepared mature oak wood; source foliage and leafy stems stay fixed."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_shoots import CrownShoots
from form_plan import validate_parents
from limb_experiment import canonical_wood
from limb_paths import dominant_paths
from oak_scaffold import tapered_radii
from outshine_ref.vegetation import Blade, Shoot
from wood_mesh import branch_tubes


def seed_continuations(parents, seed):
    """The fixed oak seed format concatenates each authored cubic's internal nodes."""
    seed = np.asarray(seed)
    validate_parents(seed)
    if len(seed) > len(parents) or not np.array_equal(parents[: len(seed)], seed):
        raise ValueError("exact original seed parent prefix required")
    result = np.full(len(parents), -1, np.int32)
    child = np.arange(1, len(seed))
    internal = child[seed[1:] == child - 1]
    result[internal - 1] = internal
    return result


def source_crown(inputs, study):
    folder = inputs / "spatial"
    with np.load(folder / "skeleton.npz") as a:
        positions, parents = a["positions"], a["parents"]
        continuations = seed_continuations(parents, a["seed_parents"])
    with np.load(folder / "tables.npz") as a:
        variants = a["leaf_variants"]
    crown = CrownShoots(
        positions,
        parents,
        variants,
        study["recipe"]["seed"],
        length_m=0.35,
        bend_rad=0.4,
        foliage_depth_m=1.1,
    )
    if crown.identity != study["products"]["spatial"]["identity"]:
        raise ValueError("prepared crown does not match source identity")
    return crown, continuations


def replace_wood(crown, original, radii, continuations):
    old = branch_tubes(crown.positions, crown.parents, radii, 8)
    nv, nf = len(old[0]), len(old[1])
    for k, v in zip(("vertices", "faces", "normals"), old, strict=True):
        np.testing.assert_array_equal(original[k][: len(v)], v)
    result = canonical_wood(
        crown, radii=radii, max_nodes=16384, continuations=continuations
    )
    if result is None:
        raise ValueError("complete continuous wood exceeds declared capacity")
    body, paths = result
    wood = {
        "vertices": np.concatenate((body["vertices"], original["vertices"][nv:])),
        "faces": np.concatenate(
            (body["faces"], original["faces"][nf:] - nv + len(body["vertices"]))
        ),
        "normals": np.concatenate((body["normals"], original["normals"][nf * 3 :])),
        "surface": np.r_[
            np.zeros(len(body["faces"]), np.uint8), original["surface"][nf:]
        ],
        "corner_uv": np.concatenate(
            (np.zeros((body["faces"].size, 2)), original["corner_uv"][nf * 3 :])
        ),
        "limb_ids": np.r_[
            body["limb_ids"],
            np.full(len(original["faces"]) - nf, np.iinfo(np.uint32).max, np.uint32),
        ],
    }
    return (
        wood,
        paths,
        {
            "old_body_vertices": nv,
            "old_body_faces": nf,
            "new_body_vertices": len(body["vertices"]),
            "new_body_faces": len(body["faces"]),
        },
    )


def export_pair(folder, output, study, entry, crown, wood, counts, continuations):
    output.mkdir(parents=True, exist_ok=False)
    for label in ("recursive", "spatial"):
        target = output / label
        target.mkdir()
        for p in folder.iterdir():
            if p.is_file():
                (target / p.name).symlink_to(p.resolve())
    target = output / "spatial"
    (target / "wood.npz").unlink()
    np.savez_compressed(target / "wood.npz", **wood)
    recipe = {
        "format": "oak-continuous-wood-1",
        "source_product_identity": entry["product_identity"],
        "source_wood_sha256": hashlib.sha256(
            (folder / "wood.npz").read_bytes()
        ).hexdigest(),
        "recipe": study["recipe"],
        "leaf_plan_identity": crown.identity,
        "along": 2,
        "sides": 8,
        "source_continuations": [
            [int(p), int(c)] for p, c in enumerate(continuations) if c >= 0
        ],
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    new = dict(entry)
    new.update(
        product_identity=identity,
        product_recipe=recipe,
        instanced_triangles=entry["instanced_triangles"]
        + counts["new_body_faces"]
        - counts["old_body_faces"],
    )
    (target / "entry.json").unlink()
    (target / "entry.json").write_text(json.dumps(new, indent=2) + "\n")
    pair = dict(study)
    pair["products"] = {
        n: dict(study["products"]["spatial"]) for n in ("recursive", "spatial")
    }
    pair["source_build_commit"] = study["commit"]
    pair["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    pair["products"]["spatial"]["wood_triangles"] = len(wood["faces"])
    pair["products"]["spatial"]["wood_array_bytes"] = sum(
        a.nbytes for a in wood.values()
    )
    pair["products"]["spatial"]["product_identity"] = identity
    pair["products"]["spatial"]["instanced_triangles"] = new["instanced_triangles"]
    pair["presentation_labels"] = {
        "recursive": "individually capped segments",
        "spatial": "continuous dominant limbs",
    }
    pair["scope"] = (
        "Same cached mature source/foliage/materials; continuous dominant limbs, separate side forks remain."
    )
    (output / "comparison.json").write_text(json.dumps(pair, indent=2) + "\n")
    return identity


def build(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private products must be under/tmp")
    began = time.perf_counter()
    study = json.loads((inputs / "comparison.json").read_text())
    folder = inputs / "spatial"
    entry = json.loads((folder / "entry.json").read_text())
    with (
        patch.object(
            Shoot, "detail", side_effect=AssertionError("unrequested foliage")
        ),
        patch.object(Blade, "mesh", side_effect=AssertionError("unrequested blades")),
        patch(
            "colonization.grow", side_effect=AssertionError("growth on prepared source")
        ),
    ):
        crown, continuations = source_crown(inputs, study)
        radii = tapered_radii(
            crown.parents,
            study["recipe"]["root_radius_m"],
            study["recipe"]["terminal_radius_m"],
        )
        paths = dominant_paths(
            crown.positions,
            crown.parents,
            radii,
            max_nodes=16384,
            continuations=continuations,
        )
        expected = sum(16 * (2 * (len(p) - 1) + 1) for p in paths)
        if expected > 262144:
            raise ValueError("body capacity before full wood IO")
        with np.load(folder / "wood.npz") as a:
            original = {k: a[k] for k in a.files}
        loaded = time.perf_counter()
        wood, paths, counts = replace_wood(crown, original, radii, continuations)
    ready = time.perf_counter()
    identity = export_pair(
        folder, output, study, entry, crown, wood, counts, continuations
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_product_identity": entry["product_identity"],
        "product_identity": identity,
        "leaf_plan_identity": crown.identity,
        "counts": counts,
        "limbs": len(paths),
        "longest_path_nodes": max(map(len, paths)),
        "path_edge_count": sum(len(p) - 1 for p in paths),
        "old_body_cap_triangles": counts["old_body_faces"] // 2,
        "new_body_cap_triangles": len(paths) * 16,
        "source_decode_and_plan_s": loaded - began,
        "continuous_body_s": ready - loaded,
        "export_s": time.perf_counter() - ready,
        "known_output_array_bytes": sum(a.nbytes for a in wood.values()),
        "known_body_plus_stem_aggregation_copy_bytes": sum(
            wood[k].nbytes for k in ("vertices", "faces", "normals")
        ),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
    }
    (output / "skin-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.inputs, args.output)
