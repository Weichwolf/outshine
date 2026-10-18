"""Verified physical prototype and stable SITE inputs; no organ-array materialization."""

import errno
import json
import os
import shutil
import zipfile
from math import prod
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from recipe_plant_operators import prepared


def read_json(path):
    if path.stat().st_size > 16 * 1024**2:
        raise ValueError("bounded prior plant JSON required")
    return json.loads(path.read_text())


def prototype_layout(path, proto):
    expected = {
        "vertices.npy": ((proto["vertices"], 3), np.dtype("f8")),
        "faces.npy": ((proto["triangles"], 3), np.dtype("u4")),
        "leaf_faces.npy": ((proto["triangles"],), np.dtype("bool")),
    }
    payload, headers = {}, 0
    with zipfile.ZipFile(path) as archive:
        if set(archive.namelist()) != set(expected):
            raise ValueError("complete typed physical prototype archive required")
        for name, (wanted_shape, wanted_dtype) in expected.items():
            with archive.open(name) as stream:
                version = np.lib.format.read_magic(stream)
                readers = {
                    (1, 0): np.lib.format.read_array_header_1_0,
                    (2, 0): np.lib.format.read_array_header_2_0,
                }
                if version not in readers:
                    raise ValueError("known NumPy physical prototype header required")
                shape, fortran, dtype = readers[version](stream)
                header = stream.tell()
            if shape != wanted_shape or fortran or dtype != wanted_dtype:
                raise ValueError("verified physical prototype shape/dtype changed")
            size = prod(shape) * dtype.itemsize
            if archive.getinfo(name).file_size != header + size:
                raise ValueError("complete physical prototype payload length required")
            payload[name] = size
            headers += header
    return payload["vertices.npy"] + payload["faces.npy"], headers


def previous(products, receipt):
    products, receipt = Path(products), Path(receipt)
    report = read_json(products / "comparison.json")
    verified = read_json(receipt)
    if (
        verified["report_sha256"] != digest(products / "comparison.json")
        or verified["producer_commit"] != report["commit"]
        or not verified["whole_form_replayed_exact"]
        or not verified["all_physical_bounds_and_SITE_products_verified"]
        or not 1 <= len(report["artifacts"]) <= 1024
    ):
        raise ValueError("verified complete prior plant/SITE product required")
    total, sizes = 0, {}
    for name, expected in report["artifacts"].items():
        path = products / name
        if not path.resolve().is_relative_to(products.resolve()):
            raise ValueError("prior plant artifact escaped its product")
        size = path.stat().st_size
        total += size
        sizes[name] = size
        if size > 128 * 1024**2 or total > 256 * 1024**2 or digest(path) != expected:
            raise ValueError("bounded immutable prior plant artifact changed")
    proto = read_json(products / "prototype/prototype.json")
    expected = {"plants.json", "landcover-ledger.json", "prototype/prototype.json"}
    expected.update("prototype/" + name for name in proto["artifacts"])
    if set(report["artifacts"]) != expected or any(
        report["artifacts"]["prototype/" + name] != sha
        for name, sha in proto["artifacts"].items()
    ):
        raise ValueError("complete physical prototype artifact ledger required")
    logical_bytes, header_bytes = prototype_layout(
        products / "prototype/complete.npz", proto
    )
    if (
        report["prototype_vertices"] != proto["vertices"]
        or report["prototype_triangles"] != proto["triangles"]
        or report["prototype_array_bytes"] != logical_bytes
    ):
        raise ValueError("verified physical prototype dimensions differ")
    records = read_json(products / "plants.json")
    if len(records) > 64 or len(records) != verified["actual_complete_plants"]:
        raise ValueError("bounded complete prior SITE ledger required")
    keys = [(r["source_part_id"], tuple(r["result"]["site_id"])) for r in records]
    if len(set(keys)) != len(keys):
        raise ValueError("unique prior source/SITE identities required")
    ledger = read_json(products / "landcover-ledger.json")
    sources = {r["id"]: r for r in ledger if "id" in r}
    if any(r["source_part_id"] not in sources for r in records):
        raise ValueError("prior SITE source provenance is incomplete")
    if any(
        (r["source_id"], r["part"])
        != (
            sources[r["source_part_id"]]["source_id"],
            sources[r["source_part_id"]]["part"],
        )
        for r in records
    ):
        raise ValueError("prior SITE source identity/part changed")
    compatible = (
        proto["definition"] == prepared().definition_fingerprint
        and proto["recipe"]["wood"]["seed"] == 718
    )
    return {
        "path": products,
        "report": report,
        "prototype": proto,
        "artifact_sizes": sizes,
        "records": records,
        "sources": sources,
        "compatible": compatible,
        "proof": {
            "products": str(products.resolve()),
            "receipt": str(receipt.resolve()),
            "report_sha256": digest(products / "comparison.json"),
            "receipt_sha256": digest(receipt),
            "checked_payload_bytes": total,
            "prototype_header_bytes_read": header_bytes,
        },
    }


def reuse_prototype(prior, output):
    if not prior["compatible"]:
        raise ValueError("matching current form definition and seed required")
    output.mkdir()
    links, copied, linked = 0, 0, 0
    artifacts = {}
    for name, expected in prior["report"]["artifacts"].items():
        if not name.startswith("prototype/"):
            continue
        source = prior["path"] / name
        destination = output / Path(name).relative_to("prototype")
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(source, destination)
            links += 1
            linked += prior["artifact_sizes"][name]
        except OSError as error:
            if error.errno != errno.EXDEV:
                raise
            shutil.copyfile(source, destination)
            copied += destination.stat().st_size
        artifacts[name] = expected
    return (
        prior["prototype"],
        artifacts,
        {
            "reused": True,
            "linked_files": links,
            "linked_payload_bytes": linked,
            "copied_payload_bytes": copied,
            "materialized_organ_array_bytes": 0,
            "logical_prototype_array_bytes": prior["report"]["prototype_array_bytes"],
        },
    )


def physical_form(output, prior=None):
    if prior is not None and prior["compatible"]:
        return reuse_prototype(prior, output)
    from stair_plant_geometry import prototype

    mesh, proto = prototype(output)
    artifacts = {"prototype/" + name: sha for name, sha in proto["artifacts"].items()}
    artifacts["prototype/prototype.json"] = digest(output / "prototype.json")
    return (
        proto,
        artifacts,
        {
            "reused": False,
            "linked_files": 0,
            "linked_payload_bytes": 0,
            "copied_payload_bytes": 0,
            "materialized_organ_array_bytes": None,
            "logical_prototype_array_bytes": mesh.vertices.nbytes
            + mesh.triangles.nbytes,
        },
    )
