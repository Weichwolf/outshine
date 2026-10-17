"""Experimental bounded numeric surface archive; admission belongs to the caller."""

import hashlib
import io
import json
import math
import re
import zipfile

import numpy as np
from outshine_ref.geometry.surface_patches import SurfacePatches

FORMAT = "outshine-finite-surface/1"
MAX_BYTES = 64 * 1024**2
MAX_META = 1024**2


def budget(value):
    if type(value) is not int or not 1 <= value <= 512 * 1024**2:
        raise ValueError(
            "explicit positive prepared-byte budget up to 512 MiB required"
        )


def pack_surface(view, *, compressed=False, max_bytes=MAX_BYTES):
    """Pack a normally validated immutable view; publish only after cold verification."""
    budget(max_bytes)
    if not isinstance(view, SurfacePatches) or type(compressed) is not bool:
        raise TypeError(
            "validated surface view and explicit compression choice required"
        )
    if not 1 <= len(view.patches) <= view.max_patches <= 4096:
        raise ValueError("bounded complete surface view required")
    arrays, rows = {}, []
    for index, part in enumerate(view.patches):
        prefix = "p" + str(index)
        arrays[prefix + "-v"] = part.mesh.vertices
        arrays[prefix + "-f"] = part.mesh.triangles
        arrays[prefix + "-active"] = view.active_faces[part.patch_id]
        for attribute, values in enumerate(part.face_data.values()):
            arrays[prefix + "-a" + str(attribute)] = values
        rows.append(
            {
                "id": part.patch_id,
                "max_faces": part.max_faces,
                "attributes": list(part.face_data),
            }
        )
    meta = {
        "format": FORMAT,
        "frame": view.patches[0].frame.identifier,
        "kind": view.patches[0].kind,
        "max_patches": view.max_patches,
        "patches": rows,
    }
    encoded = json.dumps(meta, separators=(",", ":")).encode()
    if len(encoded) > MAX_META:
        raise ValueError("prepared metadata budget exhausted")
    arrays["metadata"] = np.frombuffer(encoded, dtype=np.uint8)
    if sum(array.nbytes + 128 for array in arrays.values()) > max_bytes:
        raise ValueError("prepared expanded-array budget exhausted")
    stream = io.BytesIO()
    writer = np.savez_compressed if compressed else np.savez
    writer(stream, **arrays)
    result = stream.getvalue()
    if len(result) > max_bytes:
        raise ValueError("prepared payload-byte budget exhausted")
    return result


def inspect_archive(payload, expected_sha256, max_bytes):
    budget(max_bytes)
    if type(payload) is not bytes or not 1 <= len(payload) <= max_bytes:
        raise ValueError("bounded immutable prepared payload bytes required")
    if (
        not isinstance(expected_sha256, str)
        or re.fullmatch("[0-9a-f]{64}", expected_sha256) is None
        or hashlib.sha256(payload).hexdigest() != expected_sha256
    ):
        raise ValueError("externally admitted prepared content digest differs")
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        members = archive.infolist()
        if (
            not 1 <= len(members) <= 1 + 4096 * 11
            or len({row.filename for row in members}) != len(members)
            or sum(row.file_size for row in members) > max_bytes
        ):
            raise ValueError("bounded unique prepared array members required")
        for row in members:
            if (
                len(row.filename) > 32
                or re.fullmatch(r"(metadata|p\d+-(v|f|active|a\d+))\.npy", row.filename)
                is None
                or row.flag_bits & 1
                or row.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)
            ):
                raise ValueError(
                    "only named unencrypted numeric NPY members are supported"
                )
            with archive.open(row) as stream:
                if np.lib.format.read_magic(stream) != (1, 0):
                    raise ValueError("prepared codec requires bounded NPY 1.0 headers")
                shape, _fortran, dtype = np.lib.format.read_array_header_1_0(stream)
                if (
                    not 1 <= len(shape) <= 2
                    or any(type(n) is not int or n < 0 for n in shape)
                    or dtype.kind not in "buif"
                    or dtype.itemsize > 8
                    or dtype.fields is not None
                    or dtype.subdtype is not None
                    or math.prod(shape) * dtype.itemsize
                    != row.file_size - stream.tell()
                ):
                    raise ValueError(
                        "bounded exact primitive array lengths required before allocation"
                    )
                if row.filename == "metadata.npy" and row.file_size > MAX_META + 128:
                    raise ValueError("prepared metadata array budget exhausted")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate prepared metadata key")
        result[key] = value
    return result


def metadata(archive):
    raw = archive["metadata"]
    if raw.dtype != np.dtype("u1") or raw.ndim != 1 or len(raw) > MAX_META:
        raise ValueError("bounded byte metadata required")
    try:
        result = json.loads(raw.tobytes(), object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise ValueError("invalid prepared metadata") from error
    if type(result) is not dict or set(result) != {
        "format",
        "frame",
        "kind",
        "max_patches",
        "patches",
    }:
        raise ValueError("exact prepared metadata schema required")
    if result["format"] != FORMAT:
        raise ValueError("prepared surface codec version differs")
    return result
