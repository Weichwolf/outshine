"""Bound whole original cells and controlled placements for native composition."""

import hashlib
import json
from pathlib import Path

import numpy as np
from native_building_cell_reference import load_cells, native_parts
from parent_surface_inputs import open_parent

DATA = Path(__file__).resolve().parents[2] / "outshine/data/render/building-full-view"
ATTRIBUTES = ("positions", "normals", "uv", "uv1", "colours", "indices")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def original_inputs(data=DATA):
    request = json.loads((data / "source-plane-request.json").read_text())
    basis = np.asarray([request[k] for k in ("east", "north", "up")])
    with open_parent(data / "original-inputs.tar.gz") as parent:
        selected = np.flatnonzero(np.isin(parent.cells, request["cells"]))
    original, provenance = load_cells(
        data / "native-cells/manifest.json",
        data / "original-inputs.tar.gz",
        set(request["cells"]),
        {
            "basis": basis,
            "anchor": np.asarray(request["origin"]) @ basis.T,
            "selected_sources": selected,
        },
    )
    return native_parts(original), provenance, request


def source_bounds(parts):
    identities = np.unique(np.concatenate([p["binding"][:, 1] for p in parts]))
    if np.any(identities < 0):
        raise ValueError("whole originals require known source IDs")
    least = np.full((len(identities), 3), np.inf)
    most = np.full_like(least, -np.inf)
    for part in parts:
        slot = np.searchsorted(identities, part["binding"][:, 1])
        corners = part["positions"][part["indices"]].astype(float)
        np.minimum.at(least, slot, corners.min(1))
        np.maximum.at(most, slot, corners.max(1))
    if not np.isfinite([least, most]).all() or np.any(least > most):
        raise ValueError("finite complete source bounds required")
    return {"source_ids": identities, "source_least_m": least, "source_most_m": most}


def placements(parts, query):
    points = np.concatenate([p["positions"] for p in parts]).astype(float)
    diagonal = float(np.linalg.norm(points.max(0) - points.min(0)))
    right, _, forward = np.asarray(query["basis"])
    zero = np.zeros(3)
    separated = forward * (2 * diagonal)
    interleaved = right * (0.1 * diagonal)
    return {
        "foreground": [zero],
        "separated-background": [separated],
        "separated-original": [zero, separated],
        "interleaved-background": [interleaved],
        "interleaved-original": [zero, interleaved],
    }


def stage_mesh(root, parts, offsets, query):
    target = root / "geometry/0"
    target.mkdir(parents=True)
    records, positions = [], []
    for offset in offsets:
        for material, part in enumerate(parts):
            folder = target / str(len(records))
            folder.mkdir()
            moved = (part["positions"].astype(float) + offset).astype("<f4")
            for key in ATTRIBUTES:
                (moved if key == "positions" else part[key]).tofile(
                    folder / (key + ".bin")
                )
            positions.append(moved)
            records.append(
                {
                    "vertices": len(moved),
                    "triangles": len(part["indices"]),
                    "material": material,
                }
            )
    (target / "input.json").write_text(json.dumps({"parts": records}) + "\n")
    points = np.concatenate(positions).astype(float)
    document = {
        "least": points.min(0).tolist(),
        "most": points.max(0).tolist(),
        "catalogue": [(-np.asarray(query["basis"])[2]).tolist()],
        "queries": [query],
        "translations_m": [v.tolist() for v in offsets],
    }
    (root / "input.json").write_text(json.dumps(document, indent=2) + "\n")
    return points
