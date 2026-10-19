"""Copy one original first-in-cell source for a bounded unchanged native CPU probe.

No cache is opened or modified. Original record bytes and identities are kept;
the private manifest explicitly maps its local ordinal to the original ordinal.
The private selected-view error requests a generation variant, not a runtime profile.
"""

import argparse
import hashlib
import json
import math
import struct
import tempfile
from pathlib import Path

from building_plan_inputs import header, prepared, summary
from cached_building_streams import Reader, array, product


class NoHipReader(Reader):
    def shape(self):
        vertices = super().shape()
        self.number("i")
        self.flag()
        if self.number("B") != 0:
            raise ValueError("this bounded control supports no cached hip plan")
        return vertices


def subset_index(data, source):
    r = Reader(data)
    if (r.number("I"), r.number("I")) != (0x31425350, 8):
        raise ValueError("original index schema8 required")
    header(r)
    for width in (8, 9, 8):
        array(r, width)
    prefix = r.at
    n = r.number("Q")
    records = []
    spans = []
    for _ in range(n):
        a = r.at
        records.append(prepared(r))
        spans.append((a, r.at))
    if r.number("Q") != n:
        raise ValueError("source/header count mismatch")
    summaries = []
    for _ in range(n):
        a = r.at
        summary(r)
        summaries.append((a, r.at))
    if r.at != len(data) or not 0 <= source < n:
        raise ValueError("complete original index required")
    record = records[source]
    if any(x["cell"] == record["cell"] for x in records[:source]):
        raise ValueError("first-in-cell source required for this control")
    a, b = spans[source]
    c, d = summaries[source]
    return data[:prefix] + struct.pack("<Q", 1) + data[a:b] + struct.pack(
        "<Q", 1
    ) + data[c:d], record


def private_generation_view(data, key, error_px=0.0):
    if not math.isfinite(error_px) or not 0 <= error_px <= 4096:
        raise ValueError("bounded private generation profile required")
    product(data, key)
    body = bytearray(data[:-64])
    r = Reader(body)
    r.skip(86)
    for width in (24, 24, 4, 4, 52, 4, 4, 4, 52, 4):
        array(r, width)
    r.skip(8)
    r.flag()
    if not r.flag():
        raise ValueError("original selected view required")
    r.skip(16)
    if r.flag():
        r.skip(24)
    r.skip(8)
    struct.pack_into("<d", body, r.at, error_px)
    new_key = hashlib.sha256(body).hexdigest()
    body[22:86] = new_key.encode()
    result = bytes(body) + hashlib.sha256(body).hexdigest().encode()
    decoded = product(result, new_key)
    if decoded["view"]["error_px"] != error_px:
        raise AssertionError("private generation profile missing")
    return result, new_key


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--source", type=int, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--error-px", type=float, default=0.0)
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    original = json.loads(a.input.read_text())
    root = a.input.parent
    index, record = subset_index((root / original["index"]).read_bytes(), a.source)
    form = next(f for f in original["forms"] if f["cell"] == record["cell"])
    data = (root / form["file"]).read_bytes()
    r = NoHipReader(data)
    magic = r.number("I")
    count = r.number("Q")
    if magic != 0x33435342:
        raise ValueError("explicit source form schema3 required")
    if not count:
        raise ValueError("original nonempty form block required")
    begin, size, _, _ = r.surface()
    forms = struct.pack("<IQ", magic, 1) + data[begin : begin + size]
    view, key = private_generation_view(
        (root / original["view_file"]).read_bytes(), original["view_key"], a.error_px
    )
    out.mkdir(parents=True, exist_ok=False)
    for name, data in (
        ("subset.index", index),
        ("subset.forms", forms),
        ("fine-view.structure", view),
    ):
        (out / name).write_bytes(data)
    manifest = {
        "parent": hashlib.sha256(index + forms).hexdigest(),
        "original_parent": original["parent"],
        "original_source_ordinal": a.source,
        "original_source_id": record["source_id"],
        "local_source_ordinal": 0,
        "index": "subset.index",
        "view_key": key,
        "view_file": "fine-view.structure",
        "forms": [{"cell": record["cell"], "file": "subset.forms"}],
        "cluster_triangles": original["cluster_triangles"],
        "generation_error_px": a.error_px,
        "scope": "Private subset of original native bytes; ordinal mapping explicit; private generation profile, not original full-world cache or runtime admission.",
    }
    (out / "input.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest), flush=True)


if __name__ == "__main__":
    main()
