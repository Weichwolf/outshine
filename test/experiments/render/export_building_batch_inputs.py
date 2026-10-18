"""Freeze existing native building products read-only; no generation or cache mutation."""

import argparse
import hashlib
import json
import sqlite3
import sys
import tempfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "transport"))
from building_asset_delivery import library
from cached_building_streams import basis_sources, product
from prepared_building_residency import unpack


def payload(row, zstd):
    key, parent, offset, size, codec, native, wire, crc = row
    if zlib.crc32(wire) & 0xFFFFFFFF != crc:
        raise ValueError("native cache package CRC mismatch")
    data = unpack(zstd, wire, codec, native)
    if offset != 0 or size != len(data):
        raise ValueError("this reference requires one exact artifact per package")
    return key, parent, data


def export(cache, parents, output):
    zstd = library()
    database = sqlite3.connect(cache.resolve().as_uri() + "?mode=ro", uri=True)
    database.execute("BEGIN")
    query = """SELECT a.key,a.parent,a.offset,a.bytes,p.codec,p.native_bytes,p.bytes,p.crc
               FROM assets a JOIN packages p ON a.package=p.key WHERE """
    bases, cohorts = [], []
    for row in database.execute(query + "a.kind='building-basis' ORDER BY a.key"):
        key, parent, data = payload(row, zstd)
        anchor, sources = basis_sources(data)
        path = output / f"{key}.basis"
        path.write_bytes(data)
        bases.append(
            {
                "key": key,
                "parent": parent,
                "file": path.name,
                "anchor_ecef": anchor,
                "sha256": hashlib.sha256(data).hexdigest(),
                "sources": len(sources),
            }
        )
    for parent in parents:
        base = next(b for b in bases if b["parent"] == parent)
        _, sources = basis_sources((output / base["file"]).read_bytes())
        items, cells, views = [], [], set()
        for row in database.execute(
            query + "a.kind='building-lod' AND a.parent=? ORDER BY a.key", (parent,)
        ):
            key, _, data = payload(row, zstd)
            parsed = product(data, key)
            cell = parsed["cell"]
            if cell is None:
                continue
            if parsed["footprints"] != int((sources["cell"] == cell).sum()):
                raise ValueError(
                    "cell metadata does not match complete parent source count"
                )
            cells.append(cell)
            views.add(
                (
                    parsed["view"]["input_bytes"] if parsed["view"] else None,
                    parsed["detail"],
                )
            )
            (output / f"{key}.structure").write_bytes(data)
            items.append(
                {"key": key, "cell": cell, "sha256": hashlib.sha256(data).hexdigest()}
            )
        if (
            len(cells) != len(set(cells))
            or len(views) != 1
            or set(cells) != set(sources["cell"].tolist())
        ):
            raise ValueError("one unambiguous complete parent/view cell set required")
        cohorts.append({"parent": parent, "basis": base["key"], "products": items})
    database.close()
    manifest = {
        "basis": bases,
        "cohorts": cohorts,
        "scope": "Exact local native cache snapshot; no current-device/runtime producer equivalence inferred.",
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        json.dumps(
            {
                "basis": len(bases),
                "cohorts": len(cohorts),
                "products": sum(len(c["products"]) for c in cohorts),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--parent", action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    export(args.cache, args.parent, target)
