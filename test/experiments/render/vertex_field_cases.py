"""Unchanged native fields; archive/manifest validation precedes comparisons."""

import gzip
import hashlib
import json
from pathlib import Path

import numpy as np
from check_building_capture_inputs import check, expanded_stream
from check_building_capture_inputs import load as load_capture
from full_view_reference import load
from native_capture_meshes import chunks, mesh
from native_interpolation import validate_inputs

DATA = Path(__file__).parents[2] / "outshine/data/render"


def native_bytes(chunks_, *, vertex_spans=None):
    result = []
    offset = 0
    vertex = np.dtype([("p", "<f4", 3), ("uv", "<f4", 2), ("normal", "<u4")])
    for chunk in chunks_:
        for name in ("walls", "roofs"):
            n = len(chunk[name]["positions"])
            records = np.empty(n, vertex)
            records["p"] = np.asarray(chunk[name]["positions"], "<f4").reshape(n, 3)
            records["uv"] = np.asarray(chunk[name]["uv"], "<f4").reshape(n, 2)
            records["normal"] = np.asarray(chunk[name]["normal_words"], "<u4")
            if vertex_spans is not None:
                vertex_spans.append([offset, n, 24])
            result.append(records.tobytes())
            offset += records.nbytes
        for name in ("walls", "roofs"):
            n = len(chunk[name]["positions"])
            result.append(
                np.asarray(
                    chunk[name]["triangles"], "<u2" if n <= 65536 else "<u4"
                ).tobytes()
            )
            offset += len(result[-1])
        result.extend(
            np.asarray(chunk[name], "<f4").tobytes()
            for name in ("wall_colours", "wall_material_metres")
        )
        offset += sum(len(block) for block in result[-2:])
    return b"".join(result)


def shader_records(source):
    n = len(source["positions"])
    records = np.concatenate(
        [
            np.ascontiguousarray(source[key]).view(np.uint8).reshape(n, -1)
            for key in ("positions", "uv", "normal", "colour", "metres")
        ],
        axis=1,
    )
    return records[source["indices"]].tobytes()


def cases():
    path = DATA / "building-full-view/cell.json.gz"
    source, document, _context, receipt = load(path)
    geometry = document["results"][0]["geometry"]
    if shader_records(source) != expanded_stream(geometry, "walls") + expanded_stream(
        geometry, "roofs"
    ):
        raise ValueError("Whole-cell fields disagree with original native streams")
    spans = []
    raw = native_bytes([geometry], vertex_spans=spans)
    yield (
        "dense-shell",
        source,
        raw,
        {
            "vertex_spans": spans,
            "input": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "receipt": receipt,
            "expanded_native_sha256": hashlib.sha256(
                expanded_stream(geometry, "walls") + expanded_stream(geometry, "roofs")
            ).hexdigest(),
        },
    )
    for filename in ("native-cell.json.gz", "near-cell.json.gz"):
        path = DATA / "building-capture-movement" / filename
        document = load_capture(path)
        receipt = check(document)
        call = document["results"][0]["projection_calls"][0]
        for mode in ("Full", "Fine"):
            source = mesh(call, mode)
            parts = [
                chunk
                for row in call["capture"]["sources"]
                for chunk in chunks(row, mode)
            ]
            expected = b"".join(
                expanded_stream(chunk, name)
                for chunk in parts
                for name in ("walls", "roofs")
            )
            if shader_records(source) != expected:
                raise ValueError(
                    "Adapted vertex fields disagree with original native streams"
                )
            spans = []
            raw = native_bytes(parts, vertex_spans=spans)
            yield (
                filename.removesuffix(".json.gz") + "-" + mode,
                source,
                raw,
                {
                    "vertex_spans": spans,
                    "input": str(path),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "receipt": receipt,
                    "expanded_native_sha256": hashlib.sha256(expected).hexdigest(),
                },
            )
    directory = DATA / "interpolation/tinted-backface"
    manifest = json.loads((directory / "manifest.json").read_text())
    validate_inputs(directory, manifest)
    n = manifest["vertices"]
    source = {
        "positions": np.fromfile(directory / "stream-0.bin", "<f4").reshape(n, 3),
        "uv": np.fromfile(directory / "stream-1.bin", "<f4").reshape(n, 2),
        "normal": np.fromfile(directory / "stream-3.bin", "<u4"),
        "metres": np.fromfile(directory / "stream-2.bin", "<f4").reshape(n, 2),
        "colour": np.fromfile(directory / "colours.bin", "<f4").reshape(n, 4),
        "indices": np.frombuffer(
            gzip.decompress((directory / "indices.bin.gz").read_bytes()), "<u4"
        ).reshape(-1, 3),
    }
    raw = b"".join(
        np.ascontiguousarray(source[k]).tobytes()
        for k in ("positions", "uv", "normal", "metres", "colour")
    )
    raw += source["indices"].astype("<u2" if n <= 65536 else "<u4").tobytes()
    yield (
        "native-tinted",
        source,
        raw,
        {
            "vertex_spans": [[0, n, 12]],
            "input": str(directory),
            "manifest_sha256": hashlib.sha256(
                (directory / "manifest.json").read_bytes()
            ).hexdigest(),
            "scope": manifest["scope"],
            "expanded_native_sha256": hashlib.sha256(
                shader_records(source)
            ).hexdigest(),
        },
    )
