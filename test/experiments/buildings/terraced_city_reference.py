"""Bind this offline comparison to the earlier scoped source/physical experiment."""

import hashlib
from pathlib import Path

from terraced_mass_census import source_identity


def admitted(rows, reference, baseline):
    buildings = [r for r in rows if r["layer"] == "building"]
    sources = {r["id"]: r for r in buildings}
    original = reference["original_census"]
    if not reference["completed"] or not original:
        raise ValueError("Complete original physical comparison required")
    if (
        len(sources) != len(buildings)
        or len(sources) != len(original)
        or {r["id"] for r in original} != set(sources)
    ):
        raise ValueError("Whole original source membership changed")
    for r in original:
        if any(r[k] != v for k, v in source_identity(sources[r["id"]]).items()):
            raise ValueError("Physical reference source geometry/properties changed")
    protected = set(baseline["existing_function_ids"])
    if {
        r["id"] for r in original if r["status"] == "retained-existing-functional-owner"
    } != protected:
        raise ValueError("Existing functional ownership changed")
    proposed = {r["id"] for r in original if r["status"] == "direct-candidate"}
    records = reference["records"]
    if len(records) != len(proposed) or {r["id"] for r in records} != proposed:
        raise ValueError("Original proposed mass membership changed")
    for r in records:
        if type(r["passed"]) is not bool or any(
            r[k] != v for k, v in source_identity(sources[r["id"]]).items()
        ):
            raise ValueError("Physical source outcome changed its declared input")
    result = {r["id"]: r for r in records if r["passed"]}
    if len(result) != reference["passed"] or set(result) & protected:
        raise ValueError("Physical comparison has inconsistent admitted ownership")
    return result


def unchanged_modelling(reference):
    root = Path(__file__).resolve().parents[3]
    paths = {
        path: digest
        for path, digest in reference["implementation_sha256"].items()
        if path.startswith(
            ("test/experiments/buildings/", "lib/src/outshine_ref/buildings/")
        )
    }
    if not paths:
        raise ValueError("Physical experiment has no modelling source receipt")
    for path, expected in paths.items():
        if hashlib.sha256((root / path).read_bytes()).hexdigest() != expected:
            raise ValueError("Physical modelling source changed: " + path)
    return len(paths)
