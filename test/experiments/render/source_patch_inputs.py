"""Bind complete recorded native originals to their actual prepared-source ordinals.

This consumes the existing diagnostic binding; it supplies no live World identity.
Unknown native source-part fields remain -1, never inferred from geometry.
"""

import hashlib
import json

import numpy as np
from building_sample_originals import read_original
from compare_building_originals import native_normals
from native_building_cell_reference import load_cells, native_parts
from parent_surface_inputs import open_parent


def read_bound_sources(directory, *, binding_path=None):
    path = directory / "source-binding.json" if binding_path is None else binding_path
    if not 1 <= path.stat().st_size <= 65536:
        raise ValueError("Bounded recorded source binding required")
    raw = path.read_bytes()
    binding = json.loads(raw)
    if binding["version"] != 1 or binding["runtime_world_revision"] is not None:
        raise ValueError("Existing diagnostic binding, no invented live World revision")
    references = {}
    for key in ("source_archive", "native_cells", "original_geometry"):
        entry = binding[key]
        target = (directory / entry["path"]).resolve()
        if hashlib.sha256(target.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("Recorded source input hash changed: " + key)
        references[key] = target
    frame = binding["capture_frame"]
    basis = np.array([frame[key] for key in ("east", "north", "up")], float)
    origin = np.array(frame["origin"], float)
    world = np.array(binding["producer_world_from_field_row_major"], float).reshape(
        4, 4
    )
    anchor = np.array(binding["producer_source"]["anchor"], float)
    render_basis = basis[[0, 2, 1]] * np.array([[1], [1], [-1]])
    if (
        basis.shape != (3, 3)
        or origin.shape != (3,)
        or anchor.shape != (3,)
        or not np.isfinite(basis).all()
        or not np.isfinite(world).all()
        or not np.array_equal(origin, np.zeros(3))
        or not np.array_equal(world[:3, :3], render_basis.T)
        or not np.array_equal(world[:3, 3], anchor)
        or not np.array_equal(world[3], [0, 0, 0, 1])
    ):
        raise ValueError("Actual recorded source frame required")
    cells = binding["cells"]
    if (
        not cells
        or len(set(cells)) != len(cells)
        or any(type(c) is not int or not 1 <= c <= 64 for c in cells)
    ):
        raise ValueError("Distinct recorded source cells required")
    with open_parent(references["source_archive"]) as parent:
        if parent.parent != binding["source_archive"]["parent"]:
            raise ValueError("Actual prepared parent required")
        selected = np.flatnonzero(np.isin(parent.cells, cells))
    replay, receipt = load_cells(
        references["native_cells"],
        references["source_archive"],
        set(cells),
        {"basis": basis, "anchor": np.zeros(3), "selected_sources": selected},
    )
    replay["vertex_normal"] = native_normals(replay["normal"], basis)
    parts = native_parts(replay)
    manifest = json.loads(references["original_geometry"].read_text())
    source, provenance = read_original(
        references["original_geometry"].parent, attributes=True
    )
    if len(parts) != len(binding["parts"]) or len(parts) != len(source["parts"]):
        raise ValueError("Original material-part inventory changed")
    for index, (part, declared, original) in enumerate(
        zip(parts, binding["parts"], source["parts"], strict=True)
    ):
        if any(
            declared[key] != original[key]
            for key in ("vertices", "triangles", "material")
        ):
            raise ValueError("Original part/material binding changed")
        for key in ("positions", "normals", "uv", "uv1", "colours", "indices"):
            if (
                hashlib.sha256(part[key].tobytes()).hexdigest()
                != manifest["files"][f"{index}/{key}.bin"]["sha256"]
            ):
                raise ValueError("Whole native original words differ: " + key)
        data = part["binding"]
        contract = declared["triangle_binding"]
        if (
            data.dtype != np.dtype(contract["dtype"])
            or list(data.shape) != contract["shape"]
            or hashlib.sha256(data.tobytes()).hexdigest() != contract["sha256"]
        ):
            raise ValueError("Original prepared-source triangle binding differs")
    identities = np.concatenate([part["binding"] for part in parts])
    if (
        len(selected) != provenance["source_count"]
        or not np.array_equal(np.unique(identities[:, 1]), selected)
        or len(identities) != len(source["indices"])
    ):
        raise ValueError("Every whole original source must remain represented")
    context = {
        "binding": binding,
        "triangle_binding": identities,
        "source_ordinals": selected,
        "render_basis": render_basis,
        "source_archive": references["source_archive"],
    }
    provenance.update(
        source_binding_sha256=hashlib.sha256(raw).hexdigest(),
        native_operand_files_exact=18,
        source_archive_sha256=receipt["source_archive_sha256"],
        prepared_source_ordinals=len(selected),
        unknown_source_part_triangles=int((identities[:, 2] == -1).sum()),
        runtime_world_revision=None,
        scope="All original operand words, historical material slots and actual prepared-source triangle ordering are bound to the recorded capture frame. Prepared ordinals are not OSM IDs; unknown parts stay unknown. No live World/raster/PBR admission.",
    )
    return source, context, provenance
