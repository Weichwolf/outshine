"""Stored camera-cell surface support; original logical/caster assets stay separate."""

import json
from pathlib import Path

import numpy as np
from attributed_prisms import native_fields, native_inputs
from packed_normals import unpack
from triangle_image_reference import intersect_source, prepare_source
from viewcell_visibility import corners, select_support


def require_domain(bounds, requested):
    corners(bounds)
    requested = np.asarray(requested, float)
    if (
        requested.ndim != 2
        or requested.shape[1:] != (3,)
        or not np.isfinite(requested).all()
        or np.any(requested < np.asarray(bounds)[0])
        or np.any(requested > np.asarray(bounds)[1])
    ):
        raise ValueError(
            "camera outside supported domain: other/original product required"
        )


def compact(source, faces):
    vertices, inverse = np.unique(source["indices"][faces], return_inverse=True)
    fields = {
        k: source[k][vertices]
        for k in ("positions", "uv", "normal", "metres", "colour")
    }
    if np.all(fields["colour"] == 1):
        fields["colour"] = np.empty((0, 4), np.float32)
    if not np.any(fields["metres"]):
        fields["metres"] = np.empty((0, 2), np.float32)
    fields["indices"] = inverse.reshape(-1, 3).astype(
        np.uint16 if len(vertices) <= 65536 else np.uint32
    )
    fields["binding"] = source["binding"][faces]
    fields["original_face"] = faces.astype(np.int32)
    return fields


def cook(source, observer_box, path, provenance, profile):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    max_direction_length = float(
        np.sqrt(
            1
            + (profile["width"] / (2 * profile["focal_px"])) ** 2
            + (profile["height"] / (2 * profile["focal_px"])) ** 2
        )
    )
    support = select_support(
        source["positions"],
        source["indices"],
        observer_box,
        near_radius_m=0.1 * max_direction_length,
    )
    faces = np.flatnonzero(support.keep)
    fields = compact(source, faces)
    np.savez(path / "surfaces.npz", **fields)
    core = {
        k: a.nbytes for k, a in fields.items() if k not in ("binding", "original_face")
    }
    removed = []
    for ident in np.setdiff1d(source["binding"][:, 1], fields["binding"][:, 1]):
        own = source["binding"][:, 1] == ident
        removed.append(
            {
                "source": int(ident),
                "back_faces": int((own & (support.witness == -2)).sum()),
                "occluded_faces": int((own & (support.witness >= 0)).sum()),
                "reason": "all original faces wholly back-facing or behind an umbra",
            }
        )
    header = {
        "format": "outshine-opaque-viewcell-support-v1",
        "observer_bounds": np.asarray(observer_box).tolist(),
        "profile": profile,
        "max_direction_length": max_direction_length,
        "provenance": provenance,
        "triangles": len(faces),
        "vertices": len(fields["positions"]),
        "core_array_bytes": core,
        "core_total_bytes": sum(core.values()),
        "lineage_array_bytes": fields["binding"].nbytes
        + fields["original_face"].nbytes,
        "container_bytes": (path / "surfaces.npz").stat().st_size,
        "omitted_sources": removed,
        "work": support.work,
        "scope": "opaque primary camera visibility only; no light/caster, simulation, PBR or global residency admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return support


def load_product(path, requested_observers, provenance):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "outshine-opaque-viewcell-support-v1"
        or header["provenance"] != provenance
    ):
        raise ValueError("support format or source identity mismatch")
    # Reject unmet demand before any surface payload is opened.
    require_domain(header["observer_bounds"], requested_observers)
    with np.load(path / "surfaces.npz", allow_pickle=False) as archive:
        source = {k: archive[k] for k in archive.files}
    stored = sum(a.nbytes for a in source.values())
    source["vertex_normal"] = unpack(source["normal"])
    if not len(source["colour"]):
        source["colour"] = np.broadcast_to(
            np.ones(4, np.float32), (len(source["positions"]), 4)
        )
    if not len(source["metres"]):
        source["metres"] = np.broadcast_to(
            np.zeros(2, np.float32), (len(source["positions"]), 2)
        )
    prepared = prepare_source(source) if len(source["indices"]) else None
    inputs = native_inputs(source)
    return {
        "source": source,
        "prepared": prepared,
        "inputs": inputs,
        "header": header,
        "stored_array_bytes": stored,
        "decoded_normal_bytes": source["vertex_normal"].nbytes,
        "vertex_stage_copy_bytes": inputs["uv"].nbytes + inputs["flat_codes"].nbytes,
    }


def query(product, origins, directions):
    require_domain(product["header"]["observer_bounds"], origins)
    directions = np.asarray(directions, float)
    lengths = np.linalg.norm(directions, axis=1)
    if (
        directions.shape != np.shape(origins)
        or not np.isfinite(directions).all()
        or np.any(lengths <= 0)
        or np.any(lengths > product["header"]["max_direction_length"] * (1 + 1e-12))
    ):
        raise ValueError("query directions exceed the declared near-clip guard")
    source = product["source"]
    if product["prepared"] is None:
        depth, face = np.full(len(origins), np.inf), np.full(len(origins), -1, np.int64)
    else:
        depth, face = intersect_source(
            source, origins, directions, prepared=product["prepared"]
        )
    fields = native_fields(product["inputs"], origins, directions, depth, face)
    owner, tag = np.full(len(face), -1, np.int32), np.full(len(face), -1, np.int32)
    hit = face >= 0
    owner[hit] = source["binding"][face[hit], 1]
    tag[hit] = source["original_face"][face[hit]]
    return depth, fields, owner, tag
