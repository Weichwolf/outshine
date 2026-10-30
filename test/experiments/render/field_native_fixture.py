"""Consume #1805's pinned native point fixture without granting World authority."""

import hashlib
import json
from pathlib import Path

import numpy as np
from building_sample_originals import read_original
from field_external_depth import PointSamples, SampleBinding, validate
from field_native_points import canonical, capture_description, observer_description

ROLES = ("original", "stored", "terrain", "external", "union-terrain", "union-external")
QUERIES = ("viewer", "moved", "repeat", "light")
DATA = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-native-opponents"
)


def document(path):
    return json.loads(path.read_text())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def raw_arrays(path, manifest):
    specs = manifest["arrays"]
    expected = {
        f"{role}/{query}/{channel}"
        for role in ROLES
        for query in QUERIES
        for channel in ("rgba", "depth")
    }
    if (
        set(specs) != expected
        or path.stat().st_size > 128 * 1024**2
        or digest(path) != manifest["npz_sha256"]
    ):
        raise ValueError("Complete pinned native fixture archive required")
    keys = {v["array"] for v in specs.values()}
    physical, result = {}, {}
    with np.load(path, allow_pickle=False) as saved:
        if set(saved.files) != keys:
            raise ValueError("Exact physical native array inventory required")
        for name, spec in specs.items():
            key = spec["array"]
            shape = (512, 512, 4) if name.endswith("/rgba") else (512, 512)
            if spec["dtype"] != "float32" or tuple(spec["shape"]) != shape:
                raise ValueError(
                    "Original declared bounded native512 point layout required"
                )
            if key not in physical:
                value = saved[key]
                if (
                    value.dtype != np.dtype("<f4")
                    or value.shape != shape
                    or not np.isfinite(value).all()
                ):
                    raise ValueError("Finite declared native point array required")
                checksum = hashlib.sha256(value).hexdigest()
                value.setflags(write=False)
                physical[key] = value, checksum
            value, checksum = physical[key]
            if checksum != spec["sha256"] or value.shape != shape:
                raise ValueError("Native physical alias or point checksum changed")
            result[name] = value
    return result, {
        "logical_arrays": len(result),
        "physical_arrays": len(physical),
        "physical_array_bytes": sum(v[0].nbytes for v in physical.values()),
        "logical_array_bytes": sum(v.nbytes for v in result.values()),
    }


def frames(manifest, status, arrays, manifest_sha):
    result, checked = {}, set()
    expected_capture = capture_description(manifest["controls"]["capture"], 262144)
    for role in ROLES:
        report = manifest["reports"][role]
        if report["mode"] != "material-fields" or [
            row["query"] for row in report["cases"]
        ] != list(QUERIES):
            raise ValueError("Existing native material-fields case order required")
        for row in report["cases"]:
            query = row["query"]
            capture = capture_description(row["capture"], 262144)
            if capture != expected_capture:
                raise ValueError(
                    "Native originals and outer fields must share exact rays/clips"
                )
            observer = observer_description(row["lighting_camera"])
            prefix = f"{role}/{query}/"
            rgba, z = arrays[prefix + "rgba"], arrays[prefix + "depth"]
            revision = tuple(
                manifest["arrays"][prefix + c]["sha256"] for c in ("rgba", "depth")
            )
            # A per-case immutable capture cohort is not a live illumination or
            # World revision. Missing physical light operands remain unknown.
            cohort = hashlib.sha256(
                canonical(
                    {
                        "fixture": status["fixture_revision"],
                        "manifest": manifest_sha,
                        "case": query,
                    }
                ).encode()
            ).hexdigest()
            binding = SampleBinding(
                "native-source-frame:"
                + manifest["controls"]["original_manifest_sha256"],
                canonical(capture),
                None,
                ("whole-fixture-source:" + status["fixture_revision"] + ":" + role,),
                "recorded-material-context:" + manifest["native_binary_sha256"],
                None,
                canonical(observer),
                fixture_revision=cohort,
                sample_revision=revision,
            )
            sample = PointSamples(
                binding,
                rgba,
                z,
                np.broadcast_to(np.zeros((), float), z.shape),
                np.broadcast_to(np.ones((), bool), z.shape),
            )
            if revision not in checked:
                validate(sample, 262144)
                checked.add(revision)
            result[role, query] = sample
    return result


def read_fixture(directory=DATA):
    manifest, status = (
        document(directory / "manifest.json"),
        document(directory / "reference-status.json"),
    )
    if (
        manifest["version"] != 1
        or status["version"] != 1
        or status["admission"] != "diagnostic-only"
        or status["world_revision"] is not None
    ):
        raise ValueError(
            "This reader accepts a diagnostic fixture, never a live World claim"
        )
    if set(manifest["reports"]) != set(ROLES):
        raise ValueError(
            "All whole original, stored, outside and direct union roles required"
        )
    controls = document(directory / "controls.json")
    if controls != manifest["controls"]:
        raise ValueError(
            "Native controls changed independently of the capture manifest"
        )
    original_dir = directory.parent / "building-depth-samples/original-geometry"
    if digest(original_dir / "manifest.json") != controls["original_manifest_sha256"]:
        raise ValueError(
            "The complete unchanged original geometry identity is required"
        )
    _geometry, provenance = read_original(original_dir)
    if provenance != manifest["original"]:
        raise ValueError("Whole native original accounting changed")
    arrays, work = raw_arrays(directory / "samples.npz", manifest)
    samples = frames(manifest, status, arrays, digest(directory / "manifest.json"))
    return manifest, status, samples, work
