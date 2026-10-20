"""Cook shared sparse layered fields from original native triangles, without a layer cap."""

import json
from pathlib import Path

import numpy as np
import trimesh
from attributed_prisms import native_inputs
from layered_tree_plan import plan
from packed_normals import pack
from trimesh.ray.ray_triangle import RayMeshIntersector


class Source:
    def __init__(self, source):
        self.source = source
        self.xyz = source["positions"][source["indices"]].astype(float)
        if not 0 < len(self.xyz) <= 131072 or not np.isfinite(self.xyz).all():
            raise ValueError("bounded finite source geometry required")
        e = self.xyz[:, 1:] - self.xyz[:, :1]
        normal = np.cross(e[:, 0], e[:, 1])
        length = np.linalg.norm(normal, axis=1)
        if np.any(length == 0):
            raise ValueError("nondegenerate native triangles required")
        self.geometric = normal / length[:, None]
        self.normal = source["normal"][source["indices"][:, 0]]
        if not (source["normal"][source["indices"]] == self.normal[:, None]).all():
            raise ValueError("this field profile requires flat native shading normals")
        self.group = np.argmax(abs(normal), axis=1) * 2
        self.group += normal[np.arange(len(normal)), self.group // 2] < 0
        self.geometry_word = pack(self.geometric)
        inputs = native_inputs(source)
        self.codes = inputs["flat_codes"]
        self.values = np.concatenate(
            (inputs["uv"], source["metres"], source["colour"]), axis=1
        )[source["indices"]].astype(float)
        self.gradient = np.empty((len(self.xyz), 2, 8))
        self.indices = []
        for group in range(6):
            ids = np.flatnonzero(self.group == group)
            axis = group // 2
            other = [i for i in range(3) if i != axis]
            a = self.xyz[ids][:, 1:, other] - self.xyz[ids][:, :1, other]
            self.gradient[ids] = np.linalg.solve(
                a, self.values[ids, 1:] - self.values[ids, :1]
            )
            mesh = trimesh.Trimesh(
                source["positions"].astype(float), source["indices"][ids], process=False
            )
            if len(ids):
                _ = mesh.triangles_tree
            self.indices.append((ids, RayMeshIntersector(mesh)))
        self.work = {
            "capture_rays": 0,
            "raw_intersections": 0,
            "retained_intersections": 0,
        }

    def hits(self, box, resolution, group):
        axis = group // 2
        other = [i for i in range(3) if i != axis]
        yy, xx = np.indices((resolution, resolution))
        origin = np.empty((resolution * resolution, 3))
        origin[:, other] = (
            box[0, other]
            + (np.column_stack((xx.ravel(), yy.ravel())) + 0.5)
            * np.ptp(box, axis=0)[other]
            / resolution
        )
        direction = np.zeros(3)
        direction[axis] = -1 if group % 2 == 0 else 1
        origin[:, axis] = box[1, axis] + 1 if direction[axis] < 0 else box[0, axis] - 1
        ids, index = self.indices[group]
        self.work["capture_rays"] += len(origin)
        if not len(ids):
            return np.empty(0, np.uint16), np.empty(0, np.int64), np.empty((0, 3))
        pieces = []
        retained = 0
        for begin in range(0, len(origin), 32):
            batch = origin[begin : begin + 32]
            faces, rays = index.intersects_id(
                batch,
                np.broadcast_to(direction, batch.shape),
                return_locations=False,
                multiple_hits=True,
            )
            rays = rays + begin
            faces = ids[faces]
            self.work["raw_intersections"] += len(faces)
            n = self.geometric[faces]
            t = np.sum((self.xyz[faces, 0] - origin[rays]) * n, axis=1) / (
                n @ direction
            )
            points = origin[rays] + t[:, None] * direction
            own = ((points >= box[0]) & (points < box[1])).all(1)
            pieces.append((rays[own], faces[own], points[own]))
            retained += int(own.sum())
            if retained > 262144:
                raise ValueError("field-node sample budget exhausted")
        rays, faces, points = (np.concatenate([p[k] for p in pieces]) for k in range(3))
        order = np.lexsort((faces, points[:, axis], rays))
        self.work["retained_intersections"] += len(order)
        return rays[order].astype(np.uint16), faces[order], points[order]


def block(source, box, resolution):
    parts = []
    offsets = [0]
    for group in range(6):
        pixel, faces, points = source.hits(box, resolution, group)
        axis = group // 2
        other = [i for i in range(3) if i != axis]
        delta = points[:, other] - source.xyz[faces, 0][:, other]
        base = source.values[faces, 0] + np.einsum(
            "ni,nij->nj", delta, source.gradient[faces]
        )
        affine = np.concatenate(
            (base[:, None, :], source.gradient[faces]), axis=1
        ).astype(np.float32)
        parts.append(
            {
                "pixel": pixel,
                "depth": (points[:, axis] - box[0, axis]).astype(np.float32),
                "geometric_normal": source.geometry_word[faces],
                "normal": source.normal[faces],
                "code": source.codes[faces],
                "coordinates": affine[:, :, :2],
                "metres": affine[:, :, 2:4],
                "colour": affine[:, :, 4:],
                "source": source.source["binding"][faces, 1],
                "original_face": faces.astype(np.uint32),
            }
        )
        offsets.append(offsets[-1] + len(faces))
        if offsets[-1] > 262144:
            raise ValueError("field-node sample budget exhausted")
    result = {k: np.concatenate([p[k] for p in parts]) for k in parts[0]}
    metric = np.any(result["metres"] != 0, axis=(1, 2))
    if metric.any():
        index = np.full(len(metric), -1, np.int32)
        index[metric] = np.arange(metric.sum())
        result["metre_index"] = index
        result["metres"] = result["metres"][metric]
    else:
        result["metre_index"] = np.empty(0, np.int32)
        result["metres"] = np.empty((0, 3, 2), np.float32)
    colour = np.any(result["colour"][:, 0] != 1, axis=1) | np.any(
        result["colour"][:, 1:] != 0, axis=(1, 2)
    )
    if colour.any():
        index = np.full(len(colour), -1, np.int32)
        index[colour] = np.arange(colour.sum())
        result["colour_index"] = index
        result["colour"] = result["colour"][colour]
    else:
        result["colour_index"] = np.empty(0, np.int32)
        result["colour"] = np.empty((0, 3, 4), np.float32)
    result["offsets"] = np.array(offsets, np.uint32)
    return result


def cook(
    source,
    observer,
    profile,
    output,
    *,
    resolution=8,
    target_px=1.0,
    max_nodes=16384,
    provenance=None,
):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    prepared = Source(source)
    bounds = np.stack((prepared.xyz.min(1), prepared.xyz.max(1)), axis=1)
    tree, work = plan(
        bounds,
        observer,
        profile,
        resolution=resolution,
        target_px=target_px,
        max_nodes=max_nodes,
    )
    counts = []
    sizes = []
    archives = []
    source_counts = {int(i): 0 for i in np.unique(source["binding"][:, 1])}
    total_records = 0
    (output / "nodes").mkdir()
    for node, box in enumerate(tree.bounds):
        fields = block(prepared, box, resolution)
        if sum(a.nbytes for a in fields.values()) > 64 * 1024**2:
            raise ValueError("node field budget exceeded")
        p = output / "nodes" / f"{node:06d}.npz"
        np.savez_compressed(p, **fields)
        counts.append(len(fields["depth"]))
        total_records += len(fields["depth"])
        if total_records > 2097152:
            raise ValueError("field-tree sample budget exhausted")
        sizes.append(sum(a.nbytes for a in fields.values()))
        archives.append(p.stat().st_size)
        for i, n in zip(*np.unique(fields["source"], return_counts=True), strict=True):
            source_counts[int(i)] += int(n)
    header = {
        "bounds": tree.bounds,
        "children": tree.children,
        "resolution": np.array(resolution, np.uint32),
        "records": np.array(counts, np.uint32),
        "field_bytes": np.array(sizes, np.uint64),
        "archive_bytes": np.array(archives, np.uint64),
        "sources": np.array(sorted(source_counts), np.int32),
    }
    np.savez_compressed(output / "header.npz", **header)
    result = {
        "nodes": len(tree.bounds),
        "records": sum(counts),
        "field_bytes": sum(sizes),
        "header_bytes": sum(a.nbytes for a in header.values()),
        "archive_bytes": sum(archives) + (output / "header.npz").stat().st_size,
        "tree_work": work,
        "capture_work": prepared.work,
        "source_record_counts": source_counts,
        "unsampled_sources": [
            {"source": i, "reason": "no shared lattice sample"}
            for i, n in source_counts.items()
            if not n
        ],
        "profile": profile,
        "target_spacing_px": target_px,
        "observer_bounds": np.asarray(observer).tolist(),
        "scope": "sampling proposal; not a surface/opacity/reuse-volume certificate",
        "provenance": provenance or {},
    }
    (output / "cook.json").write_text(json.dumps(result, indent=2) + "\n")
    return result
