"""One versioned finite geometric product; frame illumination stays separate."""

import hashlib
import json

import numpy as np
from exchange_operator import validate_links


def write_exchange(folder, basis, links, samples, seed, bias):
    folder.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(
        folder / "geometry.npz",
        points=basis["points"],
        normals=basis["normals"],
        links=links,
    )
    header = {
        k: basis[k]
        for k in ("leaf_nodes", "ground_grid", "geometry_sha256", "stand_identity")
    }
    header.update(
        format="finite-face-exchange-1", samples=samples, seed=seed, bias_m=bias
    )
    identity = hashlib.sha256(json.dumps(header, sort_keys=True).encode()).hexdigest()
    header["identity"] = identity
    header["npz_sha256"] = hashlib.sha256(
        (folder / "geometry.npz").read_bytes()
    ).hexdigest()
    (folder / "geometry.json").write_text(json.dumps(header, indent=2) + "\n")
    return header


def read_exchange(folder, expected_geometry):
    header = json.loads((folder / "geometry.json").read_text())
    required = {
        "format",
        "samples",
        "seed",
        "bias_m",
        "identity",
        "npz_sha256",
        "leaf_nodes",
        "ground_grid",
        "geometry_sha256",
        "stand_identity",
    }
    if set(header) != required:
        raise ValueError("geometry-only header fields required")
    if (
        header["format"] != "finite-face-exchange-1"
        or header["geometry_sha256"] != expected_geometry
    ):
        raise ValueError(
            "matching finite geometric product and source geometry required"
        )
    identity = {k: v for k, v in header.items() if k not in ("identity", "npz_sha256")}
    if (
        hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        != header["identity"]
    ):
        raise ValueError("geometric product identity changed")
    if (
        hashlib.sha256((folder / "geometry.npz").read_bytes()).hexdigest()
        != header["npz_sha256"]
    ):
        raise ValueError("geometric product payload changed")
    with np.load(folder / "geometry.npz", allow_pickle=False) as f:
        if set(f.files) != {"points", "normals", "links"}:
            raise ValueError("geometry-only product arrays required")
        points, normals, links = f["points"], f["normals"], f["links"]
    count = header["leaf_nodes"] + header["ground_grid"] ** 2
    if (
        points.shape != (count, 3)
        or normals.shape != points.shape
        or not np.isfinite(points).all()
        or not np.isfinite(normals).all()
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-10)
        or links.shape != (2 * count, header["samples"])
    ):
        raise ValueError("complete matching collocation basis required")
    validate_links(links)
    return {**header, "points": points, "normals": normals}, links
