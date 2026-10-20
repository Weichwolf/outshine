"""A shared sparse vertical-interval field; no per-triangle or per-object render mesh."""

import json
from pathlib import Path

import numpy as np
import shapely
from column_roof_patches import bottom_m, project_patch, roof_patches, tangent_basis


def patch_columns(polygon, plane, bottom, pitch):
    low = np.floor(np.array(polygon.bounds[:2]) / pitch).astype(int)
    high = np.ceil(np.array(polygon.bounds[2:]) / pitch).astype(int)
    if np.prod(high - low) > 1048576:
        raise ValueError("patch exceeds bounded grid work")
    x, y = np.meshgrid(np.arange(low[0], high[0]), np.arange(low[1], high[1]))
    xy = np.column_stack((x.ravel(), y.ravel()))
    boxes = shapely.box(
        xy[:, 0] * pitch,
        xy[:, 1] * pitch,
        (xy[:, 0] + 1) * pitch,
        (xy[:, 1] + 1) * pitch,
    )
    clipped = shapely.intersection(boxes, polygon)
    keep = shapely.area(clipped) > 0
    clipped, xy = clipped[keep], xy[keep]
    vertices, index = shapely.get_coordinates(clipped, return_index=True)
    tops = np.full(len(xy), -np.inf)
    np.maximum.at(tops, index, vertices @ plane[:2] + plane[2])
    return xy, np.column_stack((np.full(len(xy), bottom), tops)), len(boxes)


def merge_columns(rows, source_count):
    """Union only overlapping height spans; retain all source/part contributors."""
    columns, intervals, contributors, offsets, lineage_offsets = [], [], [], [0], [0]
    for key in sorted(rows):
        columns.append(key)
        ordered = sorted(rows[key])
        low, high, source, part = ordered[0]
        owners = {(source, part)}
        for a, b, source, part in ordered[1:]:
            if a <= high:
                high = max(high, b)
                owners.add((source, part))
            else:
                intervals.append((low, high))
                contributors.extend(sorted(owners))
                lineage_offsets.append(len(contributors))
                low, high, owners = a, b, {(source, part)}
        intervals.append((low, high))
        contributors.extend(sorted(owners))
        lineage_offsets.append(len(contributors))
        offsets.append(len(intervals))
    if not columns:
        raise ValueError("no supported field columns")
    spans = np.asarray(intervals, np.float32)
    # Outward rounding avoids Float32 contracting the measured height intervals.
    spans[:, 0] = np.nextafter(spans[:, 0], np.float32(-np.inf))
    spans[:, 1] = np.nextafter(spans[:, 1], np.float32(np.inf))
    lineage = np.asarray(contributors, np.uint32)
    if lineage[:, 0].max() >= source_count:
        raise ValueError("source ordinal escapes parent")
    return {
        "columns": np.asarray(columns, np.int32),
        "offsets": np.asarray(offsets, np.uint32),
        "spans": spans,
        "lineage_offsets": np.asarray(lineage_offsets, np.uint32),
        "lineage": lineage,
    }


def cook(parent, pitch, output, *, cells=None):
    if not np.isfinite(pitch) or pitch <= 0:
        raise ValueError("positive finite projected grid pitch required")
    basis = tangent_basis(parent.plans)
    rows, needs, work = (
        {},
        [],
        {"shapes": 0, "roof_patches": 0, "cell_polygon_tests": 0, "input_intervals": 0},
    )
    source_count = len(parent.cells)
    selected = set(parent.forms if cells is None else cells)
    source_state = np.zeros(source_count, np.uint8)
    tilt = 0.0
    for _, ordinals, surfaces in parent.iter_forms(selected):
        for ordinal, surface in zip(ordinals, surfaces, strict=True):
            ordinal = int(ordinal)
            source_state[ordinal] = 1
            if surface is None:
                needs.append(
                    {
                        "source": ordinal,
                        "part": None,
                        "reason": "unsupported original form",
                    }
                )
                source_state[ordinal] = 2
                continue
            for part, shape in enumerate(surface["shapes"]):
                work["shapes"] += 1
                try:
                    patches = roof_patches(shape)
                except ValueError as error:
                    needs.append(
                        {"source": ordinal, "part": part, "reason": str(error)}
                    )
                    source_state[ordinal] = 2
                    continue
                for polygon, plane in patches:
                    if polygon.area == 0:
                        continue
                    footprint, height, bottom, shift = project_patch(
                        surface, polygon, plane, basis, bottom_m(surface, shape)
                    )
                    tilt = max(tilt, float(shift))
                    xy, spans, count = patch_columns(footprint, height, bottom, pitch)
                    work["roof_patches"] += 1
                    work["cell_polygon_tests"] += count
                    work["input_intervals"] += len(xy)
                    for key, span in zip(xy, spans, strict=True):
                        if span[1] <= span[0]:
                            raise ValueError("nonpositive original column interval")
                        rows.setdefault(tuple(key), []).append((*span, ordinal, part))
    arrays = merge_columns(rows, source_count)
    ordinals = np.flatnonzero(source_state)
    table = np.empty(
        len(ordinals),
        dtype=[
            ("ordinal", "<u4"),
            ("kind", "u1"),
            ("id", "<u8"),
            ("cell", "u1"),
            ("state", "u1"),
        ],
    )
    for at, ordinal in enumerate(ordinals):
        kind, identity = parent.plans["records"][ordinal]["source_id"]
        table[at] = (
            ordinal,
            kind,
            identity,
            parent.cells[ordinal],
            source_state[ordinal],
        )
    arrays.update(basis=basis, source_table=table)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(output / "field.npz", **arrays)
    header = {
        "schema": "shared-column-geometry-1",
        "archive_sha256": parent.archive_sha256,
        "parent": parent.parent,
        "pitch_m": pitch,
        "selected_cells": sorted(map(int, selected)),
        "source_count": source_count,
        "selected_sources": int(np.count_nonzero(source_state)),
        "represented_sources": len(np.unique(arrays["lineage"][:, 0])),
        "occupied_columns": len(arrays["columns"]),
        "height_intervals": len(arrays["spans"]),
        "columns_with_vertical_gaps": int(
            np.count_nonzero(np.diff(arrays["offsets"]) > 1)
        ),
        "intervals_with_multiple_contributors": int(
            np.count_nonzero(np.diff(arrays["lineage_offsets"]) > 1)
        ),
        "source_state_values": {
            "0": "unselected",
            "1": "all shapes represented",
            "2": "local original/finer needs; supported parts may remain",
        },
        "local_original_needs": needs,
        "max_common_up_wall_shift_m": tilt,
        "arrays": {k: a.nbytes for k, a in arrays.items()},
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "archive_bytes": (output / "field.npz").stat().st_size,
        "work": work,
        "scope": "geometry-only conservative XY cell occupancy, roof maxima; no PBR, opacity, material filtering, native GPU or universal error guarantee",
    }
    (output / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header


def store_native_fallback(output, source, header):
    """Only the supplied native cell can resolve its named local original needs."""
    needs = {r["source"] for r in header["local_original_needs"]}
    faces = np.flatnonzero(np.isin(source["binding"][:, 1], list(needs)))
    vertices, indices = np.unique(source["indices"][faces], return_inverse=True)
    arrays = {
        "positions": source["positions"][vertices],
        "indices": indices.reshape(-1, 3).astype(np.uint32),
        "vertex_normal": source["vertex_normal"][vertices].astype(np.float32),
        "binding": source["binding"][faces],
        "original_face": faces.astype(np.uint32),
    }
    if needs and set(source["binding"][faces, 1]) != needs:
        raise ValueError("native fallback does not cover every named cell source")
    np.savez_compressed(output / "fallback.npz", **arrays)
    header["native_fallback_arrays"] = {k: a.nbytes for k, a in arrays.items()}
    header["native_fallback_bytes"] = sum(header["native_fallback_arrays"].values())
    header["stored_array_bytes"] += header["native_fallback_bytes"]
    (output / "header.json").write_text(json.dumps(header, indent=2) + "\n")
