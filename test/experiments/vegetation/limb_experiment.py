"""Replace only stored growth wood; keep all source nodes, leafy stems and organ rows."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from colonization import pipe_radii
from limb_paths import dominant_paths
from long_limb_surface import piecewise_limb_mesh
from spatial_crown_cost import restore
from tree_entry import load
from wood_mesh import branch_tubes


def canonical_wood(
    crown, along=2, sides=8, max_faces=262144, *, radii=None, max_nodes=4096
):
    if radii is None:
        radii = pipe_radii(crown.parents, 0.14)
    paths = dominant_paths(crown.positions, crown.parents, radii, max_nodes=max_nodes)
    if paths is None:
        return None
    expected = sum(2 * sides * ((len(path) - 1) * along + 1) for path in paths)
    if expected > max_faces:
        return None
    vertices, faces, normals, owners = [], [], [], []
    offset = 0
    for path in paths:
        mesh = piecewise_limb_mesh(crown.positions, radii, path, along, sides)
        if mesh is None:
            return None
        vertices.append(mesh["vertices"])
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"])
        owners.append(np.full(len(mesh["faces"]), path[1], np.uint32))
        offset += len(mesh["vertices"])
    result = {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "limb_ids": np.concatenate(owners),
    }
    assert len(result["faces"]) == expected
    return result, paths


def build(inputs, output, along=2):
    start = time.perf_counter()
    crown = restore(inputs)
    metadata, products = load(inputs / "spatial")
    original = products["wood.npz"]
    baseline = branch_tubes(
        crown.positions, crown.parents, pipe_radii(crown.parents, 0.14), 8
    )
    vertex_count = len(baseline[0])
    face_count = len(baseline[1])
    for key, values in zip(("vertices", "faces", "normals"), baseline, strict=True):
        np.testing.assert_array_equal(original[key][: len(values)], values)
    built = canonical_wood(crown, along)
    if built is None:
        raise ValueError("complete limb product exceeds explicit capacity")
    body, paths = built
    wood = {
        "vertices": np.concatenate(
            (body["vertices"], original["vertices"][vertex_count:])
        ),
        "faces": np.concatenate(
            (
                body["faces"],
                original["faces"][face_count:] - vertex_count + len(body["vertices"]),
            )
        ),
        "normals": np.concatenate(
            (body["normals"], original["normals"][face_count * 3 :])
        ),
        "surface": np.zeros(
            len(body["faces"]) + len(original["faces"]) - face_count, np.uint8
        ),
    }
    wood["corner_uv"] = np.zeros((wood["faces"].size, 2))
    wood["limb_ids"] = np.r_[
        body["limb_ids"],
        np.full(
            len(original["faces"]) - face_count, np.iinfo(np.uint32).max, np.uint32
        ),
    ]
    output.mkdir(parents=True, exist_ok=False)
    for label in ("recursive", "spatial"):
        folder = output / label
        folder.mkdir()
        for path in (inputs / "spatial").iterdir():
            if path.is_file():
                (folder / path.name).symlink_to(path.resolve())
    target = output / "spatial"
    (target / "wood.npz").unlink()
    np.savez_compressed(target / "wood.npz", **wood)
    new = dict(metadata)
    new["instanced_triangles"] += len(body["faces"]) - face_count
    new["geometry_identity"] = hashlib.sha256(
        json.dumps(
            {
                "format": "dominant-limb-sweep-1",
                "source": crown.identity,
                "along": along,
                "sides": 8,
            },
            sort_keys=True,
        ).encode()
    ).hexdigest()
    new["scope"] = (
        "Same cached tree/leafy stem/organ rows; continuous dominant-axis wood only. Separate non-dominant caps, no globally welded forks or species/LOD/native admission."
    )
    (target / "entry.json").unlink()
    (target / "entry.json").write_text(json.dumps(new, indent=2) + "\n")
    children = np.bincount(crown.parents[1:], minlength=len(crown.parents))
    forks = np.flatnonzero(children >= 2)
    radii = pipe_radii(crown.parents, 0.14)
    fork = int(forks[np.argmax(radii[forks])])
    child = np.flatnonzero(crown.parents == fork)[0]
    incoming = crown.positions[fork] - crown.positions[crown.parents[fork]]
    outgoing = crown.positions[child] - crown.positions[fork]
    normal = np.cross(incoming, outgoing)
    normal /= np.linalg.norm(normal)
    centre = crown.positions[fork] + (0, 0, 0.12)
    view = {
        "eye": (centre + normal * 2.8 + (0, 0, 0.6)).tolist(),
        "target": centre.tolist(),
        "span_m": 1.2,
    }
    for name, entry in (("recursive", metadata), ("spatial", new)):
        entry = dict(entry, preview_views={"detail": view}, preview_samples=8)
        file = output / name / "entry.json"
        file.unlink()
        file.write_text(json.dumps(entry, indent=2) + "\n")
    np.savez_compressed(output / "limbs-only.npz", **body)
    (output / "limb-paths.json").write_text(json.dumps(paths) + "\n")
    report = {
        "presentation_labels": {
            "recursive": "stored independent segments",
            "spatial": "continuous dominant limbs",
        },
        "source_identity": crown.identity,
        "geometry_identity": new["geometry_identity"],
        "canonical_nodes": len(crown.parents),
        "canonical_edges": len(crown.parents) - 1,
        "limbs": len(paths),
        "old_body_faces": face_count,
        "new_body_faces": len(body["faces"]),
        "old_whole_triangles": metadata["instanced_triangles"],
        "new_whole_triangles": new["instanced_triangles"],
        "body_array_bytes": sum(a.nbytes for a in body.values()),
        "along": along,
        "detail_fork_node": fork,
        "build_s": time.perf_counter() - start,
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--along", type=int, default=2)
    args = parser.parse_args()
    build(args.inputs, args.output, args.along)
