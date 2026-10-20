"""Original v10 cluster membership, bound to unchanged native triangle identities."""

import struct
import tarfile
from collections import defaultdict, deque
from dataclasses import dataclass

import numpy as np
from check_building_full_view import check


@dataclass
class Groups:
    offsets: np.ndarray
    faces: np.ndarray
    bounds: np.ndarray
    native: np.ndarray


def from_artifact(document, original, source):
    check(document, original)
    at = len(b"outshine-structure") + 4 + 64
    lists = []
    for size in (24, 24, 4, 4, 52, 4, 4, 4, 52, 4):
        count = struct.unpack_from("<Q", original, at)[0]
        at += 8
        lists.append(original[at : at + count * size])
        at += count * size
    dtype = np.dtype(
        [("values", "<f4", 10), ("first", "<u4"), ("count", "<u4"), ("level", "<u4")]
    )
    offsets, faces, bounds, native = [0], [], [], []
    face_base = 0
    for stream, (record_list, index_list, name) in enumerate(
        ((4, 7, "walls"), (8, 9, "roofs"))
    ):
        triangles = np.array(
            document["results"][0]["geometry"][name]["triangles"], np.uint32
        )
        original_ids = defaultdict(deque)
        for index, triangle in enumerate(triangles):
            original_ids[tuple(triangle)].append(index + face_base)
        indices = np.frombuffer(lists[index_list], "<u4")
        records = np.frombuffer(lists[record_list], dtype)
        touched = np.zeros(len(indices), bool)
        for index, record in enumerate(records):
            if record["level"] != 0 or record["values"][8] != 0:
                raise ValueError(
                    "this whole-Shell control requires original level-zero clusters"
                )
            first, count = int(record["first"]), int(record["count"])
            if (
                first % 3
                or count % 3
                or not count
                or first + count > len(indices)
                or touched[first : first + count].any()
            ):
                raise ValueError("invalid or overlapping native cluster range")
            touched[first : first + count] = True
            group = []
            for triangle in indices[first : first + count].reshape(-1, 3):
                ids = original_ids[tuple(triangle)]
                if not ids:
                    raise ValueError(
                        "DAG triangle does not bind to an unused original face"
                    )
                group.append(ids.popleft())
            faces.extend(group)
            offsets.append(len(faces))
            xyz = source["positions"][source["indices"][group]].astype(float)
            bounds.append([xyz.min((0, 1)), xyz.max((0, 1))])
            native.append([stream, index, first, count, int(record["level"])])
        if not touched.all() or any(original_ids.values()):
            raise ValueError("native cluster index stream is incomplete")
        face_base += len(triangles)
    if not np.array_equal(np.sort(faces), np.arange(len(source["indices"]))):
        raise ValueError("complete disjoint original face partition required")
    return Groups(
        np.array(offsets, np.uint32),
        np.array(faces, np.uint32),
        np.array(bounds),
        np.array(native, np.uint32),
    )


def load_groups(path, document, source):
    with tarfile.open(path.parent / "original-inputs.tar.gz") as archive:
        entry = archive.getmember(
            document["parent"] + "/" + document["view_key"] + ".bin"
        )
        if not entry.isfile() or entry.size > 64 * 1024**2:
            raise ValueError("bounded original archive member required")
        original = archive.extractfile(entry).read()
    return from_artifact(document, original, source)


def selected_faces(groups, mask):
    ids = np.flatnonzero(mask)
    return (
        np.sort(
            np.concatenate(
                [groups.faces[groups.offsets[i] : groups.offsets[i + 1]] for i in ids]
            )
        )
        if len(ids)
        else np.empty(0, np.uint32)
    )
