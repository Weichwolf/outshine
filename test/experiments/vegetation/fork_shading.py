"""Unify saved collar/wood contact normals without changing any geometry."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from join_normals import smooth_join_corners
from spatial_crown_replay import check_mesh


def contacts(mesh):
    faces, kind = mesh["faces"], mesh["source_kind"]
    if (
        not 1 <= len(faces) <= 262144
        or kind.shape != (len(faces),)
        or kind.dtype.kind not in "iu"
    ):
        raise ValueError("bounded local fork and complete source-kind rows required")
    collar = np.zeros(len(mesh["vertices"]), bool)
    wood = collar.copy()
    collar[faces[kind == 2]] = True
    wood[faces[kind != 2]] = True
    return collar & wood


def shade(mesh, method="angle"):
    if method not in ("angle", "area", "uniform"):
        raise ValueError("declared corner weighting required")
    local = contacts(mesh)
    if not local.any():
        return mesh, {
            "local_vertices": 0,
            "unified_vertices": 0,
            "rejected_local_vertices": 0,
            "changed_corners": 0,
            "method": method,
        }
    result, work = smooth_join_corners(mesh, local, method)
    check_mesh(result)
    return result, work


def export(inputs, output, entry, wood, patches, method):
    recipe = entry["product_recipe"] | {
        "format": "oak-local-fork-shading-1",
        "shading": {
            "format": "collar-contact-corner-normal-1",
            "source_product_identity": entry["product_identity"],
            "source_wood_sha256": hashlib.sha256(
                (inputs / "spatial/wood.npz").read_bytes()
            ).hexdigest(),
            "method": method,
            "max_normal_angle_rad": float(np.pi / 4),
        },
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    output.mkdir(parents=True, exist_ok=False)
    for label in ("recursive", "spatial"):
        target = output / label
        target.mkdir()
        for source in (inputs / "spatial").iterdir():
            if source.is_file():
                (target / source.name).symlink_to(source.resolve())
    target = output / "spatial"
    for name in ("entry.json", "wood.npz"):
        (target / name).unlink()
    np.savez_compressed(target / "wood.npz", **wood)
    new = entry | {
        "product_identity": identity,
        "product_recipe": recipe,
        "geometry_product_identity": entry["product_identity"],
        "scope": "Same whole saved plant/geometry/materials; local contact normals unified, not geometric C1/species/LOD/native admission.",
    }
    (target / "entry.json").write_text(json.dumps(new, indent=2) + "\n")
    for node, mesh in patches:
        np.savez_compressed(output / f"patch-{node}.npz", **mesh)
    (output / "fork-plans.json").symlink_to((inputs / "fork-plans.json").resolve())
    study = json.loads((inputs / "comparison.json").read_text())
    study["products"] = {
        n: dict(study["products"]["spatial"]) for n in ("recursive", "spatial")
    }
    study["products"]["spatial"]["product_identity"] = identity
    study["source_build_commit"] = study["commit"]
    study["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    study["presentation_labels"] = {
        "recursive": "separate contact normals",
        "spatial": "unified contact normals",
    }
    study["scope"] = new["scope"]
    (output / "comparison.json").write_text(json.dumps(study, indent=2) + "\n")
    return identity


def build(inputs, output, method="angle"):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    if output.exists():
        raise FileExistsError(output)
    if method not in ("angle", "area", "uniform"):
        raise ValueError("declared corner weighting required")
    began = time.perf_counter()
    entry = json.loads((inputs / "spatial/entry.json").read_text())
    if entry["product_recipe"]["format"] != "oak-local-axis-forks-1":
        raise ValueError("prepared mature-oak fork product required")
    plans = json.loads((inputs / "fork-plans.json").read_text())
    if not 1 <= sum(len(p["nodes"]) for p in plans) <= 8:
        raise ValueError("bounded explicit prepared fork groups required")
    with np.load(inputs / "spatial/wood.npz", allow_pickle=False) as z:
        original = dict(z)
    loaded = time.perf_counter()
    wood = original | {"normals": original["normals"].copy()}
    corners = wood["normals"].reshape(-1, 3, 3)
    patches, reports = [], []
    for plan in plans:
        node = min(plan["nodes"])
        with np.load(inputs / f"patch-{node}.npz", allow_pickle=False) as z:
            mesh = dict(z)
        selected = wood["patch_nodes"] == node
        np.testing.assert_array_equal(
            wood["vertices"][wood["faces"][selected]], mesh["vertices"][mesh["faces"]]
        )
        np.testing.assert_array_equal(
            corners[selected], mesh["normals"].reshape(-1, 3, 3)
        )
        result, work = shade(mesh, method)
        corners[selected] = result["normals"].reshape(-1, 3, 3)
        patches.append((node, result))
        reports.append({"group": node, **work})
    ready = time.perf_counter()
    identity = export(inputs, output, entry, wood, patches, method)
    report = {
        "source_product_identity": entry["product_identity"],
        "product_identity": identity,
        "geometry_product_identity": entry["product_identity"],
        "method": method,
        "groups": reports,
        "wood_faces": len(wood["faces"]),
        "wood_array_bytes": sum(v.nbytes for v in wood.values()),
        "known_whole_normal_copy_bytes": original["normals"].nbytes,
        "known_patch_corner_scatter_bytes": sum(
            m["normals"].nbytes for _, m in patches
        ),
        "decode_s": loaded - began,
        "normal_s": ready - loaded,
        "export_s": time.perf_counter() - ready,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
    }
    (output / "shading-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument(
        "--method", choices=("angle", "area", "uniform"), default="angle"
    )
    args = parser.parse_args()
    build(args.inputs, args.output, args.method)
