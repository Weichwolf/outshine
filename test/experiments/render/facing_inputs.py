"""Read existing building clusters and transport bodies without exporting products."""

import hashlib
import json

import numpy as np
from cached_building_streams import basis_sources, product
from facing_intervals import length, point, sub


def checked(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"original input changed: {path}")
    return data


def buildings(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    for cohort in manifest["cohorts"]:
        spec = next(s for s in manifest["basis"] if s["key"] == cohort["basis"])
        anchor, sources = basis_sources(
            checked(directory / spec["file"], spec["sha256"])
        )
        for row in cohort["products"]:
            source = product(
                checked(directory / (row["key"] + ".structure"), row["sha256"]),
                row["key"],
            )
            if source["cell"] != row["cell"]:
                raise ValueError("original product/cell mismatch")
            for name, v, c, i in (("wall", 0, 4, 7), ("roof", 1, 8, 9)):
                streams = source["streams"]
                vertices = np.frombuffer(streams[v], dtype="<f4").reshape(-1, 6)[:, :3]
                indices = np.frombuffer(streams[i], dtype="<u4").reshape(-1, 3)
                words = np.frombuffer(streams[c], dtype="<u4").reshape(-1, 13)
                # Cache components interleave Self/Parent, unlike the GPU table.
                spheres = np.frombuffer(streams[c], dtype="<f4").reshape(-1, 13)[
                    :, [0, 2, 4, 6]
                ]
                if (words[:, 12] != 0).any():
                    raise ValueError("already selected level-0 meshes required")
                ranges = [
                    (int(first) // 3, int(size) // 3) for first, size in words[:, 10:12]
                ]
                if any(first % 3 for first in words[:, 10]) or any(
                    size > 128 for _, size in ranges
                ):
                    raise ValueError("unaligned or oversized original clusters")
                if sum(size for _, size in ranges) != len(indices):
                    raise ValueError("original cluster runs omit index references")
                coverage = np.zeros(len(indices), dtype=np.uint8)
                for first, size in ranges:
                    coverage[first : first + size] += 1
                if (coverage != 1).any():
                    raise ValueError("original cluster runs overlap or omit triangles")
                if not len(indices):
                    continue
                cell_sources = sources[sources["cell"] == row["cell"]]
                yield {
                    "group": cohort["parent"],
                    "key": row["key"] + ":" + name,
                    "provenance": {
                        "product": row,
                        "stream": name,
                        "source_ids": [
                            (int(s["id"]), int(s["kind"])) for s in cell_sources
                        ],
                        "source_scope": "cell catalog, no triangle-owner inference",
                        "material_scope": "original stream and vertex attributes, runtime eligibility unknown",
                    },
                    "vertices": vertices,
                    "indices": indices,
                    "ranges": ranges,
                    "spheres": spheres,
                    "scene": dict(anchor_ecef=anchor, **source["view"]),
                    "input_bytes": len(streams[v]) + len(streams[i]) + len(streams[c]),
                }


def transport(directory):
    manifest = json.loads((directory / "resident.json").read_text())
    if manifest["version"] != 1:
        raise ValueError("explicit supported resident reference version required")
    for row in manifest["roads"]:
        path = (directory / row["file"]).resolve()
        if not path.is_relative_to(directory.resolve()):
            raise ValueError("input escapes original directory")
        data = checked(path, row["sha256"])
        with np.load(path, allow_pickle=False) as archive:
            vertices, indices = archive["body_vertices"], archive["body_faces"]
            attributes = {
                k: {
                    "shape": list(archive[k].shape),
                    "sha256": hashlib.sha256(archive[k].tobytes()).hexdigest(),
                }
                for k in archive.files
                if k not in ("body_vertices", "body_faces")
            }
        if not np.isfinite(vertices).all() or indices.max() >= len(vertices):
            raise ValueError("invalid original reference body")
        # These ranges model a comparison only: there are no native transport clusters here.
        ranges = [
            (first, min(128, len(indices) - first))
            for first in range(0, len(indices), 128)
        ]
        spheres = []
        for first, size in ranges:
            points = vertices[indices[first : first + size]]
            centre = (
                ((points.min(axis=(0, 1)) + points.max(axis=(0, 1))) / 2)
                .astype(np.float32)
                .astype(float)
            )
            radius = length(sub(point(points), point(centre)))[1].max()
            radius = np.nextafter(np.float32(radius), np.float32(np.inf))
            spheres.append([*centre, radius])
        yield {
            "group": "transport-reference",
            "key": row["file"],
            "provenance": {
                "sha256": row["sha256"],
                "source_ids": row["source_ids"],
                "attributes": attributes,
                "source_scope": "original asset IDs and unchanged top-face ownership arrays; no inferred wall owner",
                "material_scope": "unchanged optional top-face materials; runtime eligibility unknown",
                "cluster_scope": "contiguous 128-triangle reference ranges, no native cluster/export contract",
            },
            "vertices": vertices,
            "indices": indices,
            "ranges": ranges,
            "spheres": np.array(spheres),
            "scene": None,
            "input_bytes": len(data),
        }
