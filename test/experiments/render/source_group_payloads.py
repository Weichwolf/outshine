"""Private file experiment: metadata demand precedes exact corner payload reads.

The files are experiment artifacts, not an AssetCache or runtime format. Source
attributes remain original; only geometric selection reads these corner copies.
"""

import hashlib
import os
from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import source_raster_candidates as fine
from source_cluster_bounds import group_demand
from source_hierarchy import prepare
from source_triangle_clusters import immutable, spread


def digest(value):
    return hashlib.sha256(value).digest()


@dataclass(frozen=True)
class LocalPayloads:
    groups: object
    root: np.ndarray
    witnesses: np.ndarray
    normal: bool
    vertices: int
    triangles: int
    index_dtype: np.dtype
    original_digest: bytes
    block_digests: np.ndarray
    original: object
    corners: object

    @property
    def metadata_bytes(self):
        return (
            self.groups.payload_bytes
            + self.root.nbytes
            + self.witnesses.nbytes
            + self.block_digests.nbytes
            + len(self.original_digest)
        )


def build(source, directory, *, order="source", leaf_triangles=4):
    """Copy once, including an explicit original fallback; retain no fine arrays."""
    from source_triangle_clusters import TriangleGroups

    if order not in ("source", "morton", "size"):
        raise ValueError("Declared static layout required")
    h = prepare(source, leaf_triangles=leaf_triangles)
    g = h.groups
    extent = g.bounds[:, 1].astype(float) - g.bounds[:, 0]
    permutation = np.arange(len(g.bounds))
    if order == "size":
        permutation = np.argsort((extent * extent).sum(1), kind="stable")
    elif order == "morton":
        center = g.bounds.astype(float).mean(1)
        span = center.max(0) - center.min(0)
        fraction = np.divide(
            center - center.min(0), span, out=np.zeros_like(center), where=span > 0
        )
        cell = (np.clip(fraction, 0, 1) * 1023).astype(np.uint32)
        code = (
            (spread(cell[:, 0]) << 2) | (spread(cell[:, 1]) << 1) | spread(cell[:, 2])
        )
        permutation = np.argsort(code, kind="stable")
    faces = np.concatenate(
        [g.faces[g.offsets[i] : g.offsets[i + 1]] for i in permutation]
    )
    offsets = np.r_[0, np.cumsum(np.diff(g.offsets)[permutation])]
    groups = TriangleGroups(
        immutable(faces, "<u4"),
        immutable(offsets, "<u4"),
        immutable(g.bounds[permutation], "<f4"),
    )
    directory.mkdir(parents=True, exist_ok=False)
    original, corners = directory / "original.bin", directory / "corners.bin"
    index_dtype = np.dtype("<u4")
    original_bytes = h.positions.tobytes() + h.triangles.astype(index_dtype).tobytes()
    original.write_bytes(original_bytes)
    blocks = []
    with corners.open("wb") as stream:
        for a, b in zip(groups.offsets[:-1], groups.offsets[1:], strict=True):
            block = h.positions[h.triangles[groups.faces[a:b]]].tobytes()
            stream.write(block)
            blocks.append(np.frombuffer(digest(block), np.uint8))
    return LocalPayloads(
        groups,
        h.bounds,
        h.witnesses,
        h.normal_positions,
        len(h.positions),
        len(h.triangles),
        index_dtype,
        digest(original_bytes),
        immutable(blocks, np.uint8),
        original,
        corners,
    )


class Reads:
    """Actual pread requests and returned bytes; no physical disk/cache claim."""

    def __init__(self, previous=None):
        self.requests = []
        self.previous = previous
        self.kept = {}
        self.copied_blocks = 0
        self.reused_blocks = 0

    def retain(self, layout):
        """Caller may pass exactly this request's immutable blocks to the next."""
        return PreviousDemand(layout, MappingProxyType(dict(self.kept)))

    def read(self, path, start, end, *, fd=None):
        owns_fd = fd is None
        fd = os.open(path, os.O_RDONLY) if owns_fd else fd
        try:
            value = os.pread(fd, end - start, start)
        finally:
            if owns_fd:
                os.close(fd)
        self.requests.append((path.name, start, end, len(value)))
        if len(value) != end - start:
            raise ValueError("Incomplete payload; retain original parent")
        return value

    def work(self):
        pages = set()
        for name, start, _, count in self.requests:
            if not count:
                continue
            pages.update(
                (name, at)
                for at in range(start // 4096, (start + count + 4095) // 4096)
            )
        return {
            "requests": len(self.requests),
            "requested_bytes": sum(b - a for _, a, b, _ in self.requests),
            "returned_bytes": sum(n for _, _, _, n in self.requests),
            "logical_pages_4096": len(pages),
            "reused_blocks": self.reused_blocks,
            "retained_block_bytes": sum(len(value) for value in self.kept.values()),
            "retained_block_slice_bytes_upper_bound": self.copied_blocks,
        }


@dataclass(frozen=True)
class PreviousDemand:
    layout: LocalPayloads
    blocks: object


def read_original(layout, reads):
    end = layout.vertices * 12 + layout.triangles * 3 * layout.index_dtype.itemsize
    value = reads.read(layout.original, 0, end)
    if digest(value) != layout.original_digest:
        raise ValueError("Changed original payload; retain original parent")
    return {
        "positions": np.frombuffer(value, "<f4", count=layout.vertices * 3).reshape(
            -1, 3
        ),
        "indices": np.frombuffer(
            value, layout.index_dtype, offset=layout.vertices * 12
        ).reshape(-1, 3),
    }


def read_groups(layout, keep, reads):
    """Merge adjacent selected blocks only; never read an excluded block's bytes."""
    groups = layout.groups
    wanted = np.flatnonzero(keep)
    previous = reads.previous
    old = previous.blocks if previous is not None and previous.layout is layout else {}
    reads.kept = {int(at): old[int(at)] for at in wanted if int(at) in old}
    reads.reused_blocks = len(reads.kept)
    missing = np.array([at for at in wanted if int(at) not in old], dtype=np.int64)
    split = np.flatnonzero(np.diff(missing) != 1) + 1
    fd = os.open(layout.corners, os.O_RDONLY) if len(missing) else None
    try:
        for run in np.split(missing, split):
            if not len(run):
                continue
            first, last = int(run[0]), int(run[-1]) + 1
            begin, end = int(groups.offsets[first]) * 36, int(groups.offsets[last]) * 36
            data = reads.read(layout.corners, begin, end, fd=fd)
            for at in run:
                a, b = (int(groups.offsets[j]) * 36 - begin for j in (at, at + 1))
                if digest(memoryview(data)[a:b]) != layout.block_digests[at].tobytes():
                    raise ValueError("Changed selected block; retain original parent")
                value = data[a:b]
                reads.kept[int(at)] = value
                reads.copied_blocks += len(value)
    finally:
        if fd is not None:
            os.close(fd)
    chunks = [np.frombuffer(reads.kept[int(at)], "<f4").reshape(-1, 3) for at in wanted]
    return np.concatenate(chunks) if chunks else np.empty((0, 3), "<f4")


def query(layout, view, placements, raster, *, raster_guard_px, reads=None, **options):
    raster = np.asarray(raster)
    fine.validate_query(view, placements, raster, raster_guard_px, **options)
    external_reads = reads is not None
    reads = Reads() if reads is None else reads
    keep, reason, diagnostics = group_demand(
        layout.root,
        layout.witnesses,
        layout.normal,
        layout.groups.bounds,
        view,
        placements,
        raster,
        raster_guard_px,
    )
    faces = np.empty(0, np.uint32)
    copied = projected = 0
    try:
        if reason:
            ids = np.arange(layout.triangles, dtype=np.uint32)
            cost = {"complete_model": False, "reason": reason}
        elif (
            keep is None
            or int(np.diff(layout.groups.offsets)[keep].sum()) * 3 > fine.MAX_VERTICES
        ):
            original = read_original(layout, reads)
            ids, cost = fine.select(
                original,
                view,
                placements,
                raster,
                raster_guard_px=raster_guard_px,
                **options,
            )
            projected = layout.vertices
        else:
            faces = layout.groups.faces[np.repeat(keep, np.diff(layout.groups.offsets))]
            if len(faces):
                positions = read_groups(layout, keep, reads)
                projected, copied = len(positions), positions.nbytes
                indices = np.arange(len(positions), dtype=np.uint32).reshape(-1, 3)
                local, cost = fine.select(
                    {"positions": positions, "indices": indices},
                    view,
                    placements,
                    raster,
                    raster_guard_px=raster_guard_px,
                    **options,
                )
                ids = (
                    faces[local]
                    if cost["complete_model"]
                    else np.arange(layout.triangles, dtype=np.uint32)
                )
                ids.sort()
            else:
                ids = np.empty(0, np.uint32)
                cost = {"complete_model": True, "reason": None}
    except (OSError, ValueError):
        # No partial set may replace the original parent on missing/corrupt IO.
        ids = np.arange(layout.triangles, dtype=np.uint32)
        cost = {"complete_model": False, "reason": "payload_unavailable"}
        reads.kept = {}
    return ids, {
        **cost,
        "original_group_retained": len(ids) == layout.triangles,
        "selected_triangles": len(ids),
        "qualified_gpu_raster": False,
        "payload": {
            **reads.work(),
            **diagnostics,
            "projected_vertex_rows": projected,
            "known_concatenate_copy_bytes": copied,
            "selected_group_triangles": len(faces),
            "metadata_bytes_held": layout.metadata_bytes,
            "fine_payload_bytes_held_between_queries": sum(
                len(v) for v in reads.kept.values()
            )
            if external_reads
            else 0,
        },
    }
