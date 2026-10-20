"""Finished regional source selection; no render geometry or shaded appearance."""

import json
from pathlib import Path

import numpy as np
from viewcell_product import require_domain


def store(result, records, path, provenance):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    ordered = sorted(records, key=lambda r: r["index"])
    table = np.array(
        [(r["index"], r["kind"], r["id"]) for r in ordered],
        dtype=[("ordinal", "<u4"), ("kind", "u1"), ("id", "<u8")],
    )
    if len(np.unique(table["ordinal"])) != len(table):
        raise ValueError("source ordinals must be unique within original parent")
    positions = np.searchsorted(table["ordinal"], result["sources"])
    if np.any(positions >= len(table)) or not np.array_equal(
        table["ordinal"][positions], result["sources"]
    ):
        raise ValueError("projection source is missing from original identity table")
    keep = np.ones(len(table), bool)
    keep[positions] = ~result["hidden"]
    arrays = {"sources": table, "keep_bits": np.packbits(keep, bitorder="little")}
    np.savez_compressed(path / "selection.npz", **arrays)
    header = {
        "format": "opaque-region-source-selection-1",
        "provenance": provenance,
        "observer_bounds": result["observer_box"].tolist(),
        "source_count": len(table),
        "hidden_sources": int((~keep).sum()),
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "arrays": {k: a.nbytes for k, a in arrays.items()},
        "container_bytes": (path / "selection.npz").stat().st_size,
        "scope": "opaque unchanged primary-visibility control; source identity retained; no shadow/caster, material, moving-occluder or native world admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header


def load_product(path, observers, provenance):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "opaque-region-source-selection-1"
        or header["provenance"] != provenance
    ):
        raise ValueError("regional source-selection identity mismatch")
    require_domain(header["observer_bounds"], observers)
    with np.load(path / "selection.npz", allow_pickle=False) as archive:
        table, bits = archive["sources"], archive["keep_bits"]
    if (
        len(table) != header["source_count"]
        or (np.diff(table["ordinal"].astype(np.int64)) <= 0).any()
        or len(bits) != (len(table) + 7) // 8
    ):
        raise ValueError("regional source-selection table mismatch")
    return {
        "header": header,
        "sources": table,
        "keep": np.unpackbits(bits, bitorder="little")[: len(table)].astype(bool),
    }


def select(product, observers, requested):
    require_domain(product["header"]["observer_bounds"], observers)
    requested = np.asarray(requested)
    if requested.ndim != 1 or not np.issubdtype(requested.dtype, np.integer):
        raise ValueError("one-dimensional original source ordinals required")
    table = product["sources"]["ordinal"]
    at = np.searchsorted(table, requested)
    known = at < len(table)
    known[known] &= table[at[known]] == requested[known]
    keep = np.ones(len(requested), bool)
    keep[known] = product["keep"][at[known]]
    return keep
