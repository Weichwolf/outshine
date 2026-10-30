"""Export registered native PBR/depth opponents from unchanged original operands."""

import argparse
import copy
import hashlib
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import numpy as np
from building_sample_originals import read_original

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-depth-samples"
CONTROLS = ROOT / "test/outshine/data/render/building-native-opponents/controls.json"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def document(path):
    return json.loads(path.read_text())


def original_files(directory, controls):
    original = DATA / "original-geometry"
    if digest(original / "manifest.json") != controls["original_manifest_sha256"]:
        raise ValueError("unchanged whole original geometry required")
    _, provenance = read_original(original)
    manifest = document(original / "manifest.json")
    directory.mkdir(parents=True)
    with np.load(original / "geometry.npz", allow_pickle=False) as saved:
        for name, record in manifest["files"].items():
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(saved[record["array"]].tobytes())
            if digest(target) != record["sha256"]:
                raise ValueError("original operand changed: " + name)
    return provenance


def append_mesh(directory, description, mesh):
    positions = np.asarray(mesh["positions"], dtype="<f4")
    indices = np.asarray(mesh["indices"], dtype="<u4")
    normals = np.zeros_like(positions, dtype=np.float64)
    points = positions[indices].astype(np.float64)
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    for corner in range(3):
        np.add.at(normals, indices[:, corner], cross)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    count = len(positions)
    arrays = {
        "positions": positions,
        "indices": indices,
        "normals": normals.astype("<f4"),
        "uv": np.zeros((count, 2), "<f4"),
        "uv1": np.zeros((count, 2), "<f4"),
        "colours": np.broadcast_to(np.asarray(mesh["colour"], "<f4"), (count, 4)),
    }
    part = directory / str(len(description["parts"]))
    part.mkdir()
    for name, value in arrays.items():
        if not np.isfinite(value).all():
            raise ValueError("finite declared opponent geometry required")
        (part / (name + ".bin")).write_bytes(value.tobytes())
    description["parts"].append(
        {"vertices": count, "triangles": len(indices), "material": mesh["material"]}
    )


def queries(controls):
    base = {
        "capture": controls["capture"],
        "field_lighting": "viewer",
        "lighting_camera": controls["lighting_camera"],
        "sun_elevation_deg": controls["sun_elevation_deg"],
    }
    moved = copy.deepcopy(base)
    moved["lighting_camera"]["eye"][0] += 4000
    light = {**moved, "sun_elevation_deg": base["sun_elevation_deg"] + 5}
    return [
        {**value, "name": name}
        for name, value in (
            ("viewer", base),
            ("moved", moved),
            ("repeat", base),
            ("light", light),
        )
    ]


def prepare(root, controls):
    original = root / "original/geometry/0"
    provenance = original_files(original, controls)
    source = document(DATA / "source-512/manifest.json")
    with np.load(DATA / "source-512/samples.npz", allow_pickle=False) as arrays:
        atlas = root / "stored-field.bin"
        atlas.write_bytes(arrays["stored_field_bytes"].tobytes())
    if digest(atlas) != source["stored_field_sha256"]:
        raise ValueError("unchanged native stored field required")
    declarations = {
        "original": (True, ()),
        "terrain": (False, ("terrain",)),
        "external": (False, ("terrain", "ring")),
        "union-terrain": (True, ("terrain",)),
        "union-external": (True, ("terrain", "ring")),
    }
    requests = {}
    for name, (whole, extras) in declarations.items():
        folder = root / name
        geometry = folder / "geometry/0"
        if whole and name != "original":
            shutil.copytree(original, geometry, copy_function=os.link)
        geometry.mkdir(parents=True, exist_ok=True)
        description = document(original / "input.json") if whole else {"parts": []}
        for extra in extras:
            append_mesh(geometry, description, controls["meshes"][extra])
        if name != "original":
            path = geometry / "input.json"
            path.unlink(missing_ok=True)
            path.write_text(json.dumps(description, indent=2) + "\n")
        requests[name] = {"queries": queries(controls)}
    (root / "stored").mkdir()
    requests["stored"] = {
        "queries": queries(controls),
        "atlas_file": str(atlas),
        "atlas_sha256": digest(atlas),
        "atlas_provenance": source["stored_field_provenance"],
        "view": 0,
    }
    return provenance, requests


def execute(binary, folder, request, *, failure=None):
    folder.mkdir(exist_ok=True)
    (folder / "input.json").write_text(json.dumps(request, indent=2) + "\n")
    command = [str(binary), str(folder), "--material-fields"]
    (folder / "command.json").write_text(json.dumps(command) + "\n")
    with (folder / "terminal.log").open("w") as log:
        result = subprocess.run(
            command,
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
    (folder / "terminal.exit").write_text(str(result.returncode) + "\n")
    if failure is not None:
        if (
            result.returncode == 0
            or failure not in (folder / "terminal.log").read_text()
        ):
            raise ValueError("expected native rejection missing: " + str(folder))
        return None
    result.check_returncode()
    return document(folder / "material-fields-result.json")


def read_samples(folder, query, capture):
    shape = (capture["height_px"], capture["width_px"])
    colour = np.fromfile(folder / (query + "-colour.f32"), "<f4").reshape(*shape, 4)
    depth = np.fromfile(folder / (query + "-depth.f32"), "<f4").reshape(shape)
    if not np.isfinite(colour).all() or not np.isfinite(depth).all():
        raise ValueError("finite native samples required")
    if not np.array_equal(colour[..., 3], depth > 0) or np.any(
        (depth < 0) | (depth > 1)
    ):
        raise ValueError("opaque native point coverage must match Depth32F")
    if np.any(colour[colour[..., 3] == 0]):
        raise ValueError("transparent native points must be black")
    return colour, depth


def comparisons(arrays):
    results = {}
    for query in ("viewer", "moved", "repeat", "light"):
        original = arrays["original/" + query + "/rgba"]
        stored = arrays["stored/" + query + "/rgba"]
        depth = arrays["original/" + query + "/depth"]
        results[query] = {
            "stored_mask_equal": bool(np.array_equal(original[..., 3], stored[..., 3])),
            "stored_depth_equal": bool(
                np.array_equal(depth, arrays["stored/" + query + "/depth"])
            ),
            "stored_rgb_relative_l1": float(
                np.abs(original[..., :3] - stored[..., :3]).sum(dtype=np.float64)
                / np.abs(original[..., :3]).sum(dtype=np.float64)
            ),
        }
        for name in ("terrain", "external"):
            outer = arrays[name + "/" + query + "/rgba"]
            outer_depth = arrays[name + "/" + query + "/depth"]
            union = arrays["union-" + name + "/" + query + "/rgba"]
            first = depth > outer_depth
            both = (depth > 0) & (outer_depth > 0)
            composed = np.where(first[..., None], original, outer)
            results[query][name] = {
                "rgba_equal": bool(np.array_equal(composed, union)),
                "depth_equal": bool(
                    np.array_equal(
                        np.maximum(depth, outer_depth),
                        arrays["union-" + name + "/" + query + "/depth"],
                    )
                ),
                "ties": int(np.count_nonzero(both & (depth == outer_depth))),
                "original_first": int(np.count_nonzero(both & first)),
                "external_first": int(np.count_nonzero(both & ~first)),
            }
    return results


def pack(root, arrays):
    unique, identities, records = {}, {}, {}
    for name, value in arrays.items():
        checksum = hashlib.sha256(value.tobytes()).hexdigest()
        identity = (value.dtype.str, value.shape, checksum)
        key = identities.setdefault(identity, name)
        unique.setdefault(key, value)
        records[name] = {
            "array": key,
            "shape": list(value.shape),
            "dtype": str(value.dtype),
            "sha256": checksum,
        }
    np.savez_compressed(root / "samples.npz", **unique)
    with np.load(root / "samples.npz", allow_pickle=False) as saved:
        for name, record in records.items():
            if not np.array_equal(saved[record["array"]], arrays[name]):
                raise ValueError("lossless native sample archive required")
    return records


def export(args):
    root = args.output.resolve()
    if root.is_relative_to(ROOT):
        raise ValueError("private native outputs belong in system temp")
    root.mkdir(parents=True, exist_ok=False)
    controls = document(args.controls)
    provenance, requests = prepare(root, controls)
    arrays, reports, failures = {}, {}, []
    for name, request in requests.items():
        reports[name] = execute(args.binary, root / name, request)
        for query in request["queries"]:
            rgba, depth = read_samples(root / name, query["name"], controls["capture"])
            arrays[name + "/" + query["name"] + "/rgba"] = rgba
            arrays[name + "/" + query["name"] + "/depth"] = depth
        if not np.array_equal(
            arrays[name + "/viewer/rgba"], arrays[name + "/repeat/rgba"]
        ):
            failures.append(
                name + ": identical observer repeat changed native radiance"
            )
        for query in ("moved", "repeat", "light"):
            if not np.array_equal(
                arrays[name + "/viewer/depth"], arrays[name + "/" + query + "/depth"]
            ):
                raise ValueError("PBR observer/light changed fixed capture depth")
    if np.array_equal(arrays["original/viewer/rgba"], arrays["original/moved/rgba"]):
        raise ValueError("independent PBR observer was ignored")
    capture = reports["original"]["cases"][0]["capture"]
    if any(
        case["capture"] != capture
        for report in reports.values()
        for case in report["cases"]
    ):
        raise ValueError("exact common capture/raster/clip binding required")
    for name, report in reports.items():
        for case, query in zip(report["cases"], requests[name]["queries"], strict=True):
            if case["lighting_camera"]["eye_m"] != query["lighting_camera"]["eye"]:
                raise ValueError(
                    "the effective PBR observer must match its declaration"
                )
    for name, record in document(DATA / "original-geometry/manifest.json")[
        "files"
    ].items():
        if digest(root / "original/geometry/0" / name) != record["sha256"]:
            raise ValueError("whole original operands changed during export")
    invalid = copy.deepcopy(requests["original"])
    invalid["queries"] = invalid["queries"][:1]
    invalid["queries"][0]["lighting_camera"]["basis"][0] = [
        2 * x for x in invalid["queries"][0]["lighting_camera"]["basis"][0]
    ]
    shutil.copytree(
        root / "original/geometry",
        root / "invalid-observer/geometry",
        copy_function=os.link,
    )
    execute(args.binary, root / "invalid-observer", invalid, failure="invalid basis")
    shifted = copy.deepcopy(requests["stored"])
    shifted["queries"] = shifted["queries"][:1]
    shifted["queries"][0]["capture"]["eye_m"][0] += 100
    execute(
        args.binary,
        root / "invalid-capture",
        shifted,
        failure="original capture volume",
    )
    records = pack(root, arrays)
    manifest = {
        "version": 1,
        "native_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "native_binary_sha256": digest(args.binary),
        "native_sources": {
            name: digest(ROOT / name)
            for name in (
                "test/experiments/render/BuildingRadianceField.cpp",
                "test/experiments/render/native_building_impostors.cpp",
            )
        },
        "exporter": {
            "path": str(Path(__file__).resolve().relative_to(ROOT)),
            "sha256": digest(Path(__file__).resolve()),
        },
        "requests": {
            name: {
                "input_sha256": digest(root / name / "input.json"),
                "request": document(root / name / "input.json"),
                "terminal_exit": int((root / name / "terminal.exit").read_text()),
            }
            for name in (*requests, "invalid-observer", "invalid-capture")
        },
        "host": platform.platform(),
        "controls": controls,
        "original": provenance,
        "reports": reports,
        "arrays": records,
        "comparisons": comparisons(arrays),
        "reference_failures": failures,
        "npz_sha256": digest(root / "samples.npz"),
        "scope": "Registered finite native opaque PBR samples; no continuous error bound, World admission or device-budget acceptance.",
    }
    (root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "native_processes": 8,
                "positive_queries": 24,
                "arrays": len(arrays),
                "npz_bytes": (root / "samples.npz").stat().st_size,
                "reference_failures": failures,
            }
        )
    )
    if failures:
        raise ValueError("reference verification failed; complete diagnostics retained")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--controls", type=Path, default=CONTROLS)
    options = parser.parse_args()
    options.binary = options.binary.resolve()
    export(options)
