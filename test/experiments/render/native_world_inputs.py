"""Bound executed native World/capture point fixtures, without live admission."""

import hashlib
import json
from itertools import product
from math import prod
from pathlib import Path
from zipfile import ZipFile

import numpy as np
from building_sample_inputs import full_buffer_digest
from building_sample_originals import read_original
from field_composition import opaque_points

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-world-opponents"
LAYERS = (
    "capture",
    "original",
    "terrain",
    "external",
    "union-terrain",
    "union-external",
)
POSES = ("near-direction", "revealing-side", "repeat")


def digest(value):
    return hashlib.sha256(value).hexdigest()


def clip_from_original(case):
    """Native recomputed Lens coefficients in real arithmetic, not GPU replay."""
    operands = case["lens_operands"]
    values = np.asarray(operands["view_projection_f32_column_major"], dtype=float)
    shift = np.asarray(operands["pre_view_translation_f64"], dtype=float)
    if (
        values.shape != (16,)
        or shift.shape != (3,)
        or not np.isfinite(values).all()
        or not np.isfinite(shift).all()
        or operands["jitter_px"] != [0, 0]
    ):
        raise ValueError(
            "Finite native Lens/pre-view operands and zero jitter required"
        )
    translation = np.eye(4)
    translation[:3, 3] = shift
    return values.reshape(4, 4, order="F") @ translation


def bound_archive(path, manifest):
    records = manifest["arrays"]
    names = {}
    for record in records.values():
        name = record["array"] + ".npy"
        if name in names and names[name] != record:
            raise ValueError("Aliased native array declarations differ")
        names[name] = record
    if (
        path.stat().st_size > 1024**2
        or digest(path.read_bytes()) != manifest["npz_sha256"]
    ):
        raise ValueError("Bounded unchanged native World archive required")
    with ZipFile(path) as archive:
        members = archive.infolist()
        if (
            len(members) != len(names)
            or not 1 <= len(names) <= 36
            or {m.filename for m in members} != set(names)
            or sum(m.file_size for m in members) > 8 * 1024**2
        ):
            raise ValueError(
                "Exact bounded native array inventory required before decode"
            )
        readers = {
            (1, 0): np.lib.format.read_array_header_1_0,
            (2, 0): np.lib.format.read_array_header_2_0,
        }
        for member in members:
            with archive.open(member) as stream:
                version = np.lib.format.read_magic(stream)
                if version not in readers:
                    raise ValueError("Known bounded primitive array header required")
                shape, fortran, dtype = readers[version](stream, max_header_size=1024)
                record = names[member.filename]
                if (
                    list(shape) != record["shape"]
                    or dtype != np.dtype("<f4")
                    or fortran
                    or prod(shape) * 4 != member.file_size - stream.tell()
                ):
                    raise ValueError(
                        "Exact primitive native row length required before decode"
                    )


def contexts(manifest):
    cases = {}
    for layer in LAYERS:
        execution = manifest["executions"][layer]
        queries = execution["input"]["queries"]
        report = manifest["native_reports"][layer]
        if (
            execution["exit"] != 0
            or digest((json.dumps(execution["input"], indent=2) + "\n").encode())
            != execution["input_sha256"]
            or len(queries) != 3
            or len(report["cases"]) != 3
        ):
            raise ValueError("Actual successful executed native requests required")
        for pose, query, case in zip(POSES, queries, report["cases"], strict=True):
            wanted = manifest["world_cameras"][
                "near-direction" if pose == "repeat" else pose
            ]
            effective = case["capture"]
            lighting = query["lighting_camera"]["capture"]
            if (
                query["name"] != pose
                or case["query"] != pose
                or query["field_lighting"] != "viewer"
                or query["sun_elevation_deg"] != 45
                or any(effective[k] != v for k, v in query["capture"].items())
                or any(
                    query["capture"][k] != v
                    for k, v in (
                        manifest["capture"] if layer == "capture" else wanted
                    ).items()
                    if k not in ("bytes", "format")
                )
            ):
                raise ValueError("Requested and effective capture/raster differ")
            if any(
                lighting[k] != wanted[k] or case["lighting_camera"][k] != lighting[k]
                for k in ("eye_m", "forward", "orthographic")
            ):
                raise ValueError("Actual World PBR observer differs")
            clip_from_original(case)
            cases[layer, pose] = case
    for pose in POSES:
        for layer in LAYERS[2:]:
            if (
                cases[layer, pose]["capture"] != cases["original", pose]["capture"]
                or cases[layer, pose]["lens_operands"]
                != cases["original", pose]["lens_operands"]
                or cases[layer, pose]["lighting_camera"]
                != cases["original", pose]["lighting_camera"]
            ):
                raise ValueError("External/original World point domains differ")
    return cases


def layout(manifest):
    raster = manifest["raster"]
    if (
        len(raster) != 2
        or any(type(n) is not int or not 1 <= n <= 4096 for n in raster)
        or prod(raster) > 4194304
    ):
        raise ValueError("Bounded explicit World raster required")
    expected = {"/".join(v) for v in product(LAYERS, POSES, ("rgba", "depth"))}
    if (
        set(manifest["arrays"]) != expected
        or manifest["version"] != 1
        or manifest["reference_failures"]
    ):
        raise ValueError("Complete native World reference inventory required")
    sampling = manifest["sampling"]
    if (
        sampling["origin"] != "top-left"
        or sampling["pixel_centres"] != [0.5, 0.5]
        or sampling["samples_per_pixel"] != 1
        or sampling["jitter_px"] != [0, 0]
    ):
        raise ValueError("Explicit unfiltered native centre sample convention required")
    for name, record in manifest["arrays"].items():
        layer, pose, channel = name.split("/")
        crop = manifest["crops_xyxy"][pose]
        if (
            len(crop) != 4
            or any(type(n) is not int for n in crop)
            or not 0 <= crop[0] < crop[2] <= raster[0]
            or not 0 <= crop[1] < crop[3] <= raster[1]
            or (crop[2] - crop[0]) * (crop[3] - crop[1]) > 65536
        ):
            raise ValueError("Bounded in-raster native crop required")
        camera = manifest["capture"]
        shape = (
            [camera["height_px"], camera["width_px"]]
            if layer == "capture"
            else [crop[3] - crop[1], crop[2] - crop[0]]
        )
        if channel == "rgba":
            shape += [4]
        if (
            record["shape"] != shape
            or record["dtype"] != "float32"
            or prod(shape) > 262144
        ):
            raise ValueError("Declared native primitive shape differs from raster")


def verify_points(manifest, arrays):
    for layer, pose in product(LAYERS, POSES):
        name = layer + "/" + pose + "/"
        rgba, z = arrays[name + "rgba"], arrays[name + "depth"]
        opaque_points(rgba, 65536)
        if (
            not np.isfinite(z).all()
            or np.any((z < 0) | (z > 1))
            or not np.array_equal(rgba[:, :, 3], z > 0)
        ):
            raise ValueError("Coupled native raw depth and opaque coverage required")
        for channel, value in (("rgba", rgba), ("depth", z[:, :, None])):
            native_raster = (
                manifest["raster"]
                if layer != "capture"
                else [rgba.shape[1], rgba.shape[0]]
            )
            crop = (
                manifest["crops_xyxy"][pose]
                if layer != "capture"
                else [0, 0, *native_raster]
            )
            raw = full_buffer_digest(value, [0] * value.shape[2], native_raster, crop)
            if raw != manifest["executions"][layer]["buffers"][name + channel]:
                raise ValueError(
                    "Full native buffer differs after lossless crop reconstruction"
                )
        if pose == "repeat" and any(
            not np.array_equal(arrays[name + c], arrays[layer + "/near-direction/" + c])
            for c in ("rgba", "depth")
        ):
            raise ValueError("Actual identical native repeat changed")
    for pose, outer in product(POSES, ("terrain", "external")):
        z, oz = (arrays[k + "/" + pose + "/depth"] for k in ("original", outer))
        if np.any((z > 0) & (z == oz)):
            raise ValueError("Unresolved native original depth tie")
        rgba, other = (arrays[k + "/" + pose + "/rgba"] for k in ("original", outer))
        if not np.array_equal(
            np.maximum(z, oz), arrays["union-" + outer + "/" + pose + "/depth"]
        ) or not np.array_equal(
            np.where((z > oz)[:, :, None], rgba, other),
            arrays["union-" + outer + "/" + pose + "/rgba"],
        ):
            raise ValueError("Actual native original point union differs")


def load(directory=DATA):
    path = directory / "manifest.json"
    if path.stat().st_size > 1024**2:
        raise ValueError("Bounded native World manifest required")
    manifest = json.loads(path.read_text())
    layout(manifest)
    cases = contexts(manifest)
    archive_path = directory / "samples.npz"
    bound_archive(archive_path, manifest)
    with np.load(archive_path, allow_pickle=False) as saved:
        physical = {r["array"]: None for r in manifest["arrays"].values()}
        physical = {key: saved[key] for key in physical}
        arrays = {name: physical[r["array"]] for name, r in manifest["arrays"].items()}
    for name, value in arrays.items():
        if digest(value.tobytes()) != manifest["arrays"][name]["sha256"]:
            raise ValueError("Native cropped array checksum differs")
        value.setflags(write=False)
    verify_points(manifest, arrays)
    original, provenance = read_original(
        ROOT / "test/outshine/data/render/building-depth-samples/original-geometry"
    )
    if provenance != manifest["original_geometry"]:
        raise ValueError("Whole original operand/source provenance differs")
    return manifest, arrays, cases, original
