"""Freeze original native building index, forms and selected view without cache writes."""

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
from prepared_building_residency import unpack

QUERY = """SELECT a.kind,a.parent,a.offset,a.bytes,p.codec,p.native_bytes,p.bytes,p.crc
           FROM assets a JOIN packages p ON a.package=p.key WHERE a.key=?"""
LIMIT = 64 * 1024**2


def save(database, zstd, key, kind, output, receipts):
    row = database.execute(QUERY, (key,)).fetchone()
    if row is None or row[0] != kind:
        raise ValueError(f"missing original {kind} package {key}")
    _, parent, offset, size, codec, native, wire, crc = row
    if not 0 < size <= LIMIT or not 0 < len(wire) <= LIMIT or native > LIMIT:
        raise ValueError("package exceeds native input bounds")
    if zlib.crc32(wire) & 0xFFFFFFFF != crc:
        raise ValueError("native package CRC mismatch")
    data = unpack(zstd, wire, codec, native)
    if offset != 0 or len(data) != size:
        raise ValueError("one complete original artifact per package required")
    file = key + ".bin"
    (output / file).write_bytes(data)
    receipts.append(
        {
            "key": key,
            "kind": kind,
            "parent": parent,
            "file": file,
            "sha256": hashlib.sha256(data).hexdigest(),
            "native_bytes": len(data),
            "package_crc": crc,
        }
    )
    return file, parent


def export(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        output == temporary
        or not output.is_relative_to(temporary)
        or output.is_relative_to(checkout)
        or output.exists()
    ):
        raise ValueError("fresh explicit system-temp output outside checkout required")
    output.mkdir(parents=True)
    zstd = library()
    database = sqlite3.connect(args.cache.resolve().as_uri() + "?mode=ro", uri=True)
    database.execute("BEGIN")
    for key in args.view_key:
        row = database.execute(QUERY, (key,)).fetchone()
        if row is None or row[0] != "building-lod":
            raise ValueError("existing native building-lod view required")
        parent = row[1]
        target = output / parent
        target.mkdir()
        receipts = []
        view, _ = save(database, zstd, key, "building-lod", target, receipts)
        index, _ = save(database, zstd, parent, "buildings", target, receipts)
        forms = []
        for cell in range(1, 65):
            identity = f"prepared-building-cell-1/{parent}/{cell}"
            form_key = hashlib.sha256(identity.encode()).hexdigest()
            if database.execute(QUERY, (form_key,)).fetchone() is None:
                continue
            file, form_parent = save(
                database, zstd, form_key, "building-forms", target, receipts
            )
            if form_parent != parent:
                raise ValueError("form parent mismatch")
            forms.append({"cell": cell, "file": file})
        document = {
            "parent": parent,
            "index": index,
            "view_key": key,
            "view_file": view,
            "forms": forms,
            "cluster_triangles": args.cluster_triangles,
            "native_packages": receipts,
            "scope": "Original SQLite read-only snapshot.",
        }
        (target / "input.json").write_text(json.dumps(document, indent=2) + "\n")
        print(parent, len(forms))
    database.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--view-key", action="append", required=True)
    parser.add_argument("--cluster-triangles", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    if not 0 < arguments.cluster_triangles <= 1024:
        parser.error("explicit bounded runtime cluster size required")
    export(arguments)
