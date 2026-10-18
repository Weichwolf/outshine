"""Plan and fetch pinned corpus inputs, preserving per-file licence declarations."""

import json
import os

from . import manifest as manifest_module, vendor
from .store import sha256_of_file

PROVENANCE_NAME = "provenance.json"

def plan(manifest, store):
    """What the run costs cold, published before it starts spending it."""
    rows = []
    total = 0
    for subject in manifest.subjects:
        for file in subject.files:
            cached = store.enabled and os.path.isfile(store.path(file["sha256"]))
            rows.append(
                {"subject": subject.id, "as": file["as"], "url": file["url"],
                 "member": file.get("member"), "bytes": file["bytes"], "cached": cached}
            )
            if not cached:
                total += file["bytes"]
    return {"files": rows, "bytesToFetch": total}

def fetch_subjects(manifest, store, destination, force=False):
    report = {"files": [], "licence": []}
    for subject in manifest.subjects:
        for file in subject.files:
            how, size = vendor.step(manifest.directory, "fetch").download_to_store(
                file["url"],
                file["sha256"],
                store,
                expected_bytes=file["bytes"],
                commit=subject.source.get("commit"),
                member=file.get("member"),
            )
            target = os.path.join(destination, file["as"])
            placed = "kept"
            packed = file.get("unpack")
            if packed is None:
                if force or not _matches(target, file["sha256"]):
                    store.copy_out(file["sha256"], target)
                    placed = "placed"
            elif force or not os.path.exists(target):
                vendor.step(manifest.directory, "fetch").unpack(
                    packed, store.path(file["sha256"]), target)
                placed = "unpacked"
            report["files"].append(
                {"subject": subject.id, "as": file["as"], "bytes": size, "source": how, "destination": placed}
            )
        report["licence"].extend(_licences(subject))
    return report

def _licences(subject):
    rows = []
    for file in subject.files:
        grants = file["licence"]
        if not isinstance(grants, list):
            grants = [grants]
        rows.append({"subject": subject.id, "as": file["as"],
                     "spdx": sorted(set(grant["spdx"] for grant in grants)),
                     "verifiedAgainst": "manifest statedAt"})
    return rows

def write_provenance(manifest, store, destination, report):
    document = {
        "manifestId": manifest.id,
        "manifestSchemaVersion": manifest_module.SCHEMA_VERSION,
        "contentStore": store.directory,
        "report": report,
    }
    path = os.path.join(destination, PROVENANCE_NAME)
    with open(path, "w") as f:
        json.dump(document, f, indent=2, sort_keys=True)
        f.write("\n")
    return path

def _matches(path, sha256):
    return os.path.isfile(path) and sha256_of_file(path) == sha256
