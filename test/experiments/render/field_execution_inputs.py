"""Bind the recorded #1805 execution requests to their finite diagnostic fixture."""

import hashlib
import json

import numpy as np
from field_native_points import (
    canonical,
    capture_description,
    observer_description,
    vector,
)


def request_parameters(query, report):
    if set(query) != {
        "capture",
        "field_lighting",
        "lighting_camera",
        "sun_elevation_deg",
        "name",
    }:
        raise ValueError("Complete fixed native fixture query operands required")
    capture = capture_description(query["capture"], 262144)
    if capture != capture_description(report["capture"], 262144):
        raise ValueError("Executed request and reported native rays differ")
    camera = query["lighting_camera"]
    if set(camera) != {"eye", "basis", "focal_px"}:
        raise ValueError("Complete executed fixture camera schema required")
    basis = np.asarray(camera["basis"], dtype=float)
    focal = camera.get("focal_px")
    if (
        basis.shape != (3, 3)
        or not np.isfinite(basis).all()
        or not np.allclose(basis @ basis.T, np.eye(3), rtol=0, atol=1e-9)
        or type(focal) not in (float, int)
        or not np.isfinite(focal)
        or focal <= 0
    ):
        raise ValueError("Unmodified complete executed perspective camera required")
    observer = {
        "eye_m": vector(camera["eye"]).tolist(),
        "forward": basis[2].tolist(),
        "orthographic": False,
    }
    if (
        observer_description(observer)
        != observer_description(report["lighting_camera"])
        or observer["forward"] != report["lighting_camera"]["forward"]
        or report["lighting_camera"]["orthographic"] is not False
    ):
        raise ValueError("Executed request and effective native observer differ")
    sun = query.get("sun_elevation_deg")
    if (
        query.get("field_lighting") != "viewer"
        or type(sun) not in (int, float)
        or not np.isfinite(sun)
        or not -90 <= sun <= 90
    ):
        raise ValueError("Finite executed native PBR/light operands required")
    return {key: value for key, value in query.items() if key != "name"}


def read_execution_inputs(directory, manifest, manifest_sha):
    path = directory / "execution-inputs.json"
    data = json.loads(path.read_text())
    if (
        data.get("version") != 1
        or data.get("fixture_manifest_sha256") != manifest_sha
        or any(
            data.get(key) != manifest[key]
            for key in ("native_commit", "native_binary_sha256")
        )
        or "world_revision" not in data
        or "light_revision" not in data
        or data["world_revision"] is not None
        or data["light_revision"] is not None
    ):
        raise ValueError(
            "Recorded diagnostic execution identity required; no live revisions"
        )
    exporter = data["exporter"]
    if (
        exporter.get("source_commit") != data["native_commit"]
        or exporter.get("path")
        != "test/experiments/render/export_native_field_opponents.py"
        or not isinstance(exporter.get("source_sha256"), str)
        or len(exporter["source_sha256"]) != 64
        or "runtime_sha256" not in exporter
        or exporter["runtime_sha256"] is not None
    ):
        raise ValueError(
            "Historical exporter source identity and unknown runtime hash required"
        )
    requests = data["requests"]
    if set(requests) != {*manifest["reports"], "invalid-observer", "invalid-capture"}:
        raise ValueError("All eight actual execution requests required")
    result, by_case = {}, {}
    for role, record in requests.items():
        request = record["request"]
        raw = (json.dumps(request, indent=2, allow_nan=False) + "\n").encode()
        terminal = record.get("terminal_exit")
        if (
            hashlib.sha256(raw).hexdigest() != record["input_sha256"]
            or type(terminal) is not int
        ):
            raise ValueError("Exact recorded request bytes and terminal exit required")
        if role.startswith("invalid-"):
            if terminal == 0 or len(request["queries"]) != 1:
                raise ValueError("Recorded invalid native request must remain rejected")
            continue
        if terminal != 0:
            raise ValueError("Positive native request did not finish successfully")
        cases = manifest["reports"][role]["cases"]
        if [query["name"] for query in request["queries"]] != [
            row["query"] for row in cases
        ]:
            raise ValueError("Executed queries must match the actual native report")
        for query, report in zip(request["queries"], cases, strict=True):
            parameters = request_parameters(query, report)
            key = query["name"]
            identity = canonical(parameters)
            if key in by_case and by_case[key] != identity:
                raise ValueError(
                    "Original/stored/outside unions require the same executed operands"
                )
            by_case[key] = identity
            result[role, key] = parameters
    return result, {
        "execution_inputs_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "execution_inputs_file_bytes": path.stat().st_size,
        "positive_processes": len(manifest["reports"]),
        "negative_processes": 2,
        "positive_queries": len(result),
        "request_sha256": {key: row["input_sha256"] for key, row in requests.items()},
        "exporter": exporter,
        "world_revision": None,
        "light_revision": None,
    }
