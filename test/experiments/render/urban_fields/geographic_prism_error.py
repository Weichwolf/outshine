"""Compare regular patch placement with upright WGS84 prism vertices (#1102)."""

import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
sys.path[:0] = [str(ROOT / "lib/src"), str(ROOT / "test/experiments/buildings")]
from outshine_ref.places.camera import ecef
from urban_patches import load

WGS84_A = 6378137.0
WGS84_E2 = 6.6943799901413165e-3


def frame_axes(phi, longitude):
    sine, cosine = np.sin(phi), np.cos(phi)
    sl, cl = np.sin(longitude), np.cos(longitude)
    zero = np.zeros_like(sine)
    return np.stack(
        (
            np.stack((-sl, cl, zero), -1),
            np.stack((-sine * cl, -sine * sl, cosine), -1),
            np.stack((cosine * cl, cosine * sl, sine), -1),
        ),
        1,
    )


def geographic(projected):
    return (
        np.arctan(np.sinh(projected[:, 1] / WGS84_A)),
        (projected[:, 0] / WGS84_A + np.pi) % (2 * np.pi) - np.pi,
    )


def metric(phi, height):
    d = 1 - WGS84_E2 * np.sin(phi) ** 2
    prime = WGS84_A / np.sqrt(d)
    meridian = WGS84_A * (1 - WGS84_E2) / d**1.5
    return np.column_stack(
        (
            np.cos(phi) * (prime + height) / WGS84_A,
            np.cos(phi) * (meridian + height) / WGS84_A,
        )
    )


def reference(product):
    k, row, column = np.nonzero(product.tiers)
    if not len(k):
        raise ValueError("at least one occupied reference prism required")
    cells = product.chunks[k] * product.side + np.column_stack((column, row))
    pitch, fill = float(product.frame[3]), float(product.frame[4])
    projected = (cells + 0.5) * pitch
    phi, longitude = geographic(projected)
    return {
        "cells": cells,
        "height": product.heights[product.tiers[k, row, column]],
        "projected": projected,
        "phi": phi,
        "longitude": longitude,
        "axes": frame_axes(phi, longitude),
        "width": metric(phi, 0) * pitch * fill,
        "low": product.chunks.min(0) * product.side,
        "high": (product.chunks.max(0) + 1) * product.side,
        "pitch": pitch,
        "fill": fill,
    }


def measure(source, side, ground, height_reference):
    start = source["low"] + ((source["cells"] - source["low"]) // side) * side
    shape = np.minimum(side, source["high"] - start)
    anchor_xy = (start + shape * 0.5) * source["pitch"]
    phi, longitude = geographic(anchor_xy)
    basis = frame_axes(phi, longitude)
    anchor = ecef(np.rad2deg(phi), np.rad2deg(longitude), height_reference)
    centre = ecef(np.rad2deg(source["phi"]), np.rad2deg(source["longitude"]), ground)
    local_xy = np.einsum("nci,ni->nc", basis[:, :2], centre - anchor)
    scale = metric(phi, height_reference)
    delta_xy = (source["projected"] - anchor_xy) * scale - local_xy
    delta = np.einsum("nci,nc->ni", basis[:, :2], delta_xy)
    half = scale * (source["pitch"] * source["fill"] * 0.5)
    east = (
        basis[:, 0] * half[:, 0, None]
        - source["axes"][:, 0] * (source["width"][:, 0] * 0.5)[:, None]
    )
    north = (
        basis[:, 1] * half[:, 1, None]
        - source["axes"][:, 1] * (source["width"][:, 1] * 0.5)[:, None]
    )
    roof = (basis[:, 2] - source["axes"][:, 2]) * source["height"][:, None]
    error = np.zeros(len(source["cells"]))
    for x in (-1, 1):
        for y in (-1, 1):
            for top in (0, 1):
                error = np.maximum(
                    error,
                    np.linalg.norm(delta + x * east + y * north + top * roof, axis=1),
                )
    return {
        "side": side,
        "patches": len(np.unique(start, axis=0)),
        "ground_height_m": ground,
        "reference_height_m": height_reference,
        "max_vertex_error_m": float(error.max()),
        "p99_vertex_error_m": float(np.quantile(error, 0.99)),
        "max_up_vector_difference": float(
            np.linalg.norm(basis[:, 2] - source["axes"][:, 2], axis=1).max()
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if (
        not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output.exists()
    ):
        parser.error("--output must be a fresh system-temp file")
    source = reference(load(args.input))
    rows = []
    for side in (8, 32, 128, 1024):
        for ground, height_reference in (
            (0, 0),
            (9000, 0),
            (9000, 9000),
            (0, 1000),
            (2000, 1000),
        ):
            row = measure(source, side, ground, height_reference)
            rows.append(row)
            print(json.dumps(row), flush=True)
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "cells": len(source["cells"]),
        "corner_pairs_per_case": len(source["cells"]) * 8,
        "scope": "Double geographic correspondence; no DEM, Float32, renderer or SSE admission",
        "rows": rows,
    }
    output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
