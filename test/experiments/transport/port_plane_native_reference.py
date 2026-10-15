"""Export or verify fixed PR181 field answers; never solve or render a new plan."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from outshine_ref.transport import PortPlaneField
from port_height_library_comparison import load
from port_plane_fixture import REFERENCE, load_reference

MAGIC = b"OSPTREF1"
STEM = "Vienna-pr181-fields"
ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def field_arrays(field, coefficients):
    return {
        "plane_origins": field.origins,
        "plane_coefficients": coefficients.reshape(-1, 3),
        "plane_ids": field.plane_ids,
        "cut_centres": field.cut_centres,
        "inward_normals": field.inward_normals,
        "scales": field.scales,
    }


def field_of(data):
    return PortPlaneField(
        data["plane_origins"],
        data["plane_ids"],
        data["cut_centres"],
        data["inward_normals"],
        data["scales"],
    )


def answers(field, xy, parts, coefficients, datum):
    height, gradient = field.evaluate(xy, parts, coefficients)
    return np.column_stack((height + datum, gradient))


def bounds(field, data, coefficients):
    result = np.empty((len(data["source_faces"]), 2))
    for start in range(0, len(result), 256):
        end = min(start + 256, len(result))
        grade, chord = field.bounds(
            data["road_xy"][data["source_faces"][start:end]],
            data["face_parts"][start:end],
            coefficients,
            data["grade_frames"][start:end],
        )
        result[start:end] = np.column_stack((grade, chord))
    return result


def port_answers(field, data, coefficients, datum):
    xy = np.broadcast_to(
        data["port_points"][:, None], (len(data["port_points"]), 2, 3, 2)
    )
    parts = np.repeat(data["port_parts"][:, :, None], 3, axis=2)
    return answers(
        field, xy.reshape(-1, 2), parts.ravel(), coefficients, datum
    ).reshape(-1, 2, 3, 3)


def collect(saved):
    product, captured, metadata, _sources = load_reference()
    field = field_of(saved)
    coefficients = saved["coefficients"][: 3 * len(field.origins)]
    points = product["vertices"][product["port_edges"]]
    data = field_arrays(field, coefficients) | {
        "datum_m": np.asarray(saved["datum_m"]),
        "road_xy": product["vertices"],
        "source_faces": product["indices"],
        "source_face_ids": np.arange(len(product["indices"])),
        "face_parts": product["face_owners"],
        "module_ids": np.arange(len(product["part_roles"])),
        "part_roles": product["part_roles"],
        "grade_frames": captured["road_frames"],
        "port_ids": np.arange(len(product["port_edges"])),
        "port_edges": product["port_edges"],
        "port_parts": product["port_parts"],
        "port_points": np.stack(
            (points[:, 0], points.mean(axis=1), points[:, 1]), axis=1
        ),
        "face_points": product["vertices"][product["indices"]].mean(axis=1),
    }
    data["face_values"] = answers(
        field, data["face_points"], data["face_parts"], coefficients, saved["datum_m"]
    )
    data["face_bounds"] = bounds(field, data, coefficients)
    data["port_values"] = port_answers(field, data, coefficients, saved["datum_m"])
    return data, metadata["frame"]


def write_binary(path, data):
    sections = {}
    with path.open("xb") as stream:
        stream.write(MAGIC)
        for name, values in data.items():
            array = np.asarray(values)
            if array.dtype.kind in "iu":
                if (
                    array.min(initial=0) < 0
                    or array.max(initial=0) > np.iinfo(np.uint32).max
                ):
                    raise ValueError("reference indices require exact UInt32")
                dtype = "<u4"
            else:
                if array.dtype.kind != "f" or not np.isfinite(array).all():
                    raise ValueError("finite Float64 reference data required")
                dtype = "<f8"
            stream.write(b"\0" * (-stream.tell() % 8))
            block = np.asarray(array, dtype=dtype).tobytes(order="C")
            sections[name] = {
                "offset": stream.tell(),
                "bytes": len(block),
                "dtype": dtype,
                "shape": list(array.shape),
            }
            stream.write(block)
    return sections


def export(source, output):
    report, saved = load(source)
    verification = json.loads((source / "verification.json").read_text())
    if (
        not verification["valid"]
        or verification["mesh_sha256"] != report["mesh_sha256"]
    ):
        raise ValueError("independently verified installed plan required")
    inputs = {
        name: report["input_sha256"][name]
        for name in ("Vienna.npz", "metadata.json", "sources.json.gz")
    }
    for name, pin in inputs.items():
        if digest(REFERENCE / name) != pin:
            raise ValueError("original versioned input differs from installed plan")
    for name, pin in report["library_sha256"].items():
        if digest(ROOT / name) != pin:
            raise ValueError(
                "original library kernels changed before reference extraction"
            )
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
        cwd=ROOT,
        text=True,
    ).strip()
    if dirty:
        raise ValueError("clean exporter source commit required")
    if output.exists() and any(output.iterdir()):
        raise ValueError("empty external output required; no pin overwrites")
    output.mkdir(parents=True, exist_ok=True)
    data, frame = collect(saved)
    path = output / (STEM + ".bin")
    sections = write_binary(path, data)
    metadata = {
        "format": "outshine-port-field-reference-v1",
        "magic": MAGIC.decode(),
        "binary": path.name,
        "bytes": path.stat().st_size,
        "sha256": digest(path),
        "arrays": sections,
        "frame": frame,
        "plan_generation_commit": report["generation_commit"],
        "plan_sha256": report["mesh_sha256"],
        "verification_sha256": digest(source / "verification.json"),
        "report_sha256": digest(source / "report.json"),
        "export_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "exporter_sha256": digest(Path(__file__)),
        "input_sha256": inputs,
        "library_sha256": report["library_sha256"],
        "numpy_version": np.__version__,
        "scope": "Fixed installed PR181 field, all original road faces/modules and whole ports. Original coarse triangle bounds may exceed final mesh targets. Analytic samples are not physical surface contacts. No new solve, refinement, rendering or native/device acceptance.",
    }
    (output / (STEM + ".json")).write_text(json.dumps(metadata, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(output),
                "bytes": metadata["bytes"],
                "sha256": metadata["sha256"],
            }
        )
    )


def read_reference(directory):
    metadata = json.loads((directory / (STEM + ".json")).read_text())
    path = directory / (STEM + ".bin")
    if (
        metadata["binary"] != path.name
        or path.stat().st_size != metadata["bytes"]
        or digest(path) != metadata["sha256"]
    ):
        raise ValueError("fixed binary reference bytes changed")
    binary = path.read_bytes()
    if not binary.startswith(MAGIC) or len(binary) > 16 * 1024 * 1024:
        raise ValueError("bounded v1 binary reference required")
    data, end = {}, len(MAGIC)
    for name, section in sorted(
        metadata["arrays"].items(), key=lambda item: item[1]["offset"]
    ):
        offset, size = section["offset"], section["bytes"]
        dtype = np.dtype(section["dtype"])
        count = int(np.prod(section["shape"], dtype=np.int64))
        if (
            dtype.str not in ("<u4", "<f8")
            or offset % 8
            or offset < end
            or offset + size > len(binary)
            or size != count * dtype.itemsize
        ):
            raise ValueError("invalid reference array section")
        data[name] = np.frombuffer(
            binary, dtype=dtype, count=count, offset=offset
        ).reshape(section["shape"])
        end = offset + size
    if end != len(binary):
        raise ValueError("unexpected trailing reference data")
    return metadata, data


def verify(directory):
    metadata, data = read_reference(directory)
    product, captured, original, _sources = load_reference()
    if metadata["frame"] != original["frame"]:
        raise ValueError("source metric frame changed")
    for name, pin in metadata["input_sha256"].items():
        if digest(REFERENCE / name) != pin:
            raise ValueError("fixed original source bytes changed")
    for name, expected in (
        ("road_xy", product["vertices"]),
        ("source_faces", product["indices"]),
        ("face_parts", product["face_owners"]),
        ("part_roles", product["part_roles"]),
        ("grade_frames", captured["road_frames"]),
        ("port_edges", product["port_edges"]),
        ("port_parts", product["port_parts"]),
    ):
        np.testing.assert_array_equal(data[name], expected)
    for name, count in (
        ("module_ids", len(product["part_roles"])),
        ("source_face_ids", len(product["indices"])),
        ("port_ids", len(product["port_edges"])),
    ):
        np.testing.assert_array_equal(data[name], np.arange(count))
    np.testing.assert_array_equal(np.unique(data["face_parts"]), data["module_ids"])
    np.testing.assert_array_equal(
        data["face_points"], data["road_xy"][data["source_faces"]].mean(axis=1)
    )
    ends = data["road_xy"][data["port_edges"]]
    np.testing.assert_array_equal(
        data["port_points"],
        np.stack((ends[:, 0], ends.mean(axis=1), ends[:, 1]), axis=1),
    )
    field = field_of(data)
    coefficients, datum = data["plane_coefficients"].ravel(), data["datum_m"]
    values = answers(
        field, data["face_points"], data["face_parts"], coefficients, datum
    )
    certificates = bounds(field, data, coefficients)
    ports = port_answers(field, data, coefficients, datum)
    np.testing.assert_allclose(
        values[:, 0], data["face_values"][:, 0], atol=1e-8, rtol=0
    )
    np.testing.assert_allclose(
        values[:, 1:], data["face_values"][:, 1:], atol=1e-10, rtol=0
    )
    np.testing.assert_allclose(certificates, data["face_bounds"], atol=1e-9, rtol=1e-9)
    np.testing.assert_allclose(ports, data["port_values"], atol=1e-8, rtol=0)
    np.testing.assert_allclose(ports[:, 0, :, 0], ports[:, 1, :, 0], atol=1e-7, rtol=0)
    np.testing.assert_allclose(
        ports[:, 0, :, 1:], ports[:, 1, :, 1:], atol=1e-10, rtol=0
    )
    normalized = np.einsum("nij,nj->ni", data["grade_frames"], values[:, 1:])
    if np.any(np.linalg.norm(normalized, axis=1) > certificates[:, 0] + 1e-9):
        raise ValueError("stored whole-face grade bound misses a centroid")
    print(
        json.dumps(
            {
                "valid": True,
                "modules": len(data["module_ids"]),
                "source_faces": len(data["source_faces"]),
                "whole_ports": len(data["port_ids"]),
                "bytes": metadata["bytes"],
                "sha256": metadata["sha256"],
                "maximum_height_replay_error_m": float(
                    abs(values[:, 0] - data["face_values"][:, 0]).max()
                ),
                "maximum_gradient_replay_error": float(
                    abs(values[:, 1:] - data["face_values"][:, 1:]).max()
                ),
                "maximum_bound_replay_error": float(
                    abs(certificates - data["face_bounds"]).max()
                ),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    writer = sub.add_parser("export")
    writer.add_argument("--source", type=Path, required=True)
    writer.add_argument("--output", type=Path, required=True)
    reader = sub.add_parser("verify")
    reader.add_argument("--reference", type=Path, default=REFERENCE / "native")
    args = parser.parse_args()
    if args.command == "export":
        export(args.source.resolve(), args.output.resolve())
    else:
        verify(args.reference.resolve())


if __name__ == "__main__":
    main()
