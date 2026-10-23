"""Actual-view source material-coordinate diagnostics; no reconstructed PBR shader."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from compare_directed_groups import retained_subset, transformed
from compare_vertex_cluster_meshes import ready_source, setup_view
from full_view_reference import load
from native_capture_meshes import fields_at
from packed_normals import unpack
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from source_material_mesh import carry_fields
from triangle_image_reference import average_samples, intersect_source, prepare_source


def coordinate_colour(uv, metres, kind):
    """False colour is a field reader, never a material/lighting approximation."""
    # The ray reader interpolates in Float64. Decode after the shader-input cast,
    # so a constant program word does not underflow from a barycentric roundoff.
    uv, metres = uv.astype(np.float32), metres.astype(np.float32)
    u, v = uv.T
    codes = np.where(u < 0, u, np.floor(u / 256))
    if kind == "program":
        phase = codes[:, None] * np.array([0.73, 1.17, 2.13])
        return 0.2 + 0.75 * (0.5 + 0.5 * np.sin(phase))
    xy = (
        np.column_stack((np.where(u < 0, 0, u - 256 * codes), v))
        if kind == "uv"
        else metres
    )
    phase = xy - np.floor(xy)
    return np.column_stack((phase, (np.floor(xy).sum(1) % 2) * 0.65 + 0.2))


def field_errors(reference, candidate):
    common = (reference["face"] >= 0) & (candidate["face"] >= 0)
    result = {"common_hit_rays": int(common.sum())}
    for key in ("uv", "metres"):
        delta = abs(reference[key][common] - candidate[key][common])
        result[key + "_maximum_absolute_error"] = delta.max(0, initial=0).tolist()

    def code(values):
        u = values[:, 0].astype(np.float32)
        return np.where(u < 0, u, np.floor(u / 256))

    result["shader_cast_program_mismatch_rays"] = int(
        (code(reference["uv"][common]) != code(candidate["uv"][common])).sum()
    )
    qualified = common & reference["shading_valid"] & candidate["shading_valid"]
    a, b = reference["normal"][qualified], candidate["normal"][qualified]
    angle = np.degrees(
        np.arctan2(np.linalg.norm(np.cross(a, b), axis=1), np.einsum("ij,ij->i", a, b))
    )
    result.update(
        qualified_normal_rays=int(qualified.sum()),
        maximum_normal_angle_degrees=float(angle.max(initial=0)),
        normal_angle_percentiles_degrees=np.quantile(angle, [0.5, 0.95, 0.99]).tolist()
        if len(angle)
        else None,
        normal_rays_over_one_degree=int((angle > 1).sum()),
    )
    return result


def sample_fields(source, origins, directions, *, qualified_faces=None):
    start = time.perf_counter()
    prepared = prepare_source(source)
    prepared_at = time.perf_counter()
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    fields = fields_at(source, origins, directions, depth, faces)
    qualified = faces >= 0
    if qualified_faces is not None:
        qualified[qualified] = qualified_faces[faces[qualified]]
    fields["normal"][~qualified] = 0
    fields["shading_valid"] = qualified
    return fields | {"depth": depth, "face": faces}, {
        "prepare_seconds": prepared_at - start,
        "intersection_and_fields_seconds": time.perf_counter() - prepared_at,
        "hit_rays": int((faces >= 0).sum()),
    }


def diagnostic_pixels(fields, kind, width, height):
    if kind == "normal":
        values = (fields["normal"].astype(np.float32) + np.float32(1)) / np.float32(2)
        values[~fields["shading_valid"]] = [1, 0, 1]
    elif kind == "colour":
        values = np.clip(fields["colour"].astype(np.float32), 0, 1)
    else:
        values = coordinate_colour(fields["uv"], fields["metres"], kind)
    values[fields["face"] < 0] = 0
    return np.rint(255 * average_samples(values, width, height, 2)).astype(np.uint8)


def ready_attributed_source(value, retained):
    """Expand kept corner fields for the diagnostic reader, without GPU-layout claims."""
    source = ready_source(value, retained)
    for field in ("uv", "metres"):
        key = "corner_" + field
        if key in value:
            source[field] = np.concatenate((value[key].reshape(-1, 2), retained[field]))
    validity = value.get("face_shading_valid", np.ones(len(value["indices"]), bool))
    qualified = np.r_[validity, np.ones(len(retained["indices"]), bool)]
    count = len(validity) * 3
    if "corner_shading_words" in value:
        normal = source["vertex_normal"][:count].reshape(-1, 3, 3)
        # Unknown faces retain a reader placeholder, never a qualified normal.
        normal[validity] = (
            unpack(value["corner_shading_words"][validity].reshape(-1))
            @ value["basis"].T
        ).reshape(-1, 3, 3)
        source["vertex_normal"][:count] = normal.reshape(-1, 3)
    if "corner_colour" in value:
        source["colour"][:count] = value["corner_colour"].reshape(-1, 4)
    elif "uniform_colour" in value:
        source["colour"][:count] = value["uniform_colour"]
    return source, qualified


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    arrays = {}
    for name in ("control", "centre"):
        with np.load(args.product / f"{name}.npz", allow_pickle=False) as archive:
            arrays[name] = {k: archive[k] for k in archive.files}
    a, b = arrays["control"], arrays["centre"]
    for key in (
        "basis",
        "anchor",
        "face_shading_valid",
        "face_native_stream",
        "uniform_colour",
    ):
        if (key in a) != (key in b) or (
            key in a and not np.array_equal(a[key], b[key])
        ):
            raise ValueError(
                "original shading frame, validity and defaults must survive placement"
            )
    restored = carry_fields(b, b["corner_swap"])
    for key in ("corner_uv", "corner_metres", "corner_shading_words", "corner_colour"):
        if key in a and not np.array_equal(
            restored[key].view(np.uint32), a[key].view(np.uint32)
        ):
            raise ValueError(
                "exact corner fields must survive clustering and winding changes"
            )
    mapped = b["input_vertex_map"][a["indices"]]
    mapped[b["corner_swap"]] = mapped[b["corner_swap"]][:, [0, 2, 1]]
    if not np.array_equal(mapped, b["indices"]):
        raise ValueError("geometry and material corner labels must agree")
    native, document, context, _ = load(args.input)
    original = transformed(native, a["basis"], a["anchor"])
    retained = retained_subset(original, a["needs_sources"])
    if not np.array_equal(
        np.sort(np.r_[a["accepted_sources"], a["needs_sources"]]),
        np.unique(native["binding"][:, 1]),
    ):
        raise ValueError("all original sources must remain accounted for")
    sources = {"original": original}
    qualified_faces = {"original": np.ones(len(original["indices"]), bool)}
    for name, value in arrays.items():
        sources[name], qualified_faces[name] = ready_attributed_source(value, retained)
    support = {"positions": np.concatenate([s["positions"] for s in sources.values()])}
    view, _, box, origins, directions = setup_view(
        support, a, document, context, {"direction": [0.45, -0.5, 0.7]}
    )
    width, height = box[1] - box[0]
    raw, costs = {}, {}
    for name, source in sources.items():
        raw[name], costs[name] = sample_fields(
            source, origins, directions, qualified_faces=qualified_faces[name]
        )
        costs[name]["unqualified_shading_hit_rays"] = int(
            ((raw[name]["face"] >= 0) & ~raw[name]["shading_valid"]).sum()
        )
    kinds = ("program", "uv", "metres")
    if "corner_shading_words" in a:
        kinds += ("normal", "colour")
    names = ("original", "control", "centre")
    zoom = 8
    pw, ph = int(width * zoom + 16), int(height * zoom + 36)
    contact = Image.new("RGB", (pw * len(kinds), ph * 4), (28, 28, 28))
    draw = ImageDraw.Draw(contact)
    comparisons = {}
    for column, kind in enumerate(kinds):
        images = {
            name: diagnostic_pixels(raw[name], kind, width, height) for name in names
        }
        for row, name in enumerate(names):
            image = Image.fromarray(images[name])
            image.save(args.output / f"{name}-{kind}.png")
            contact.paste(
                image.resize(
                    (int(width * zoom), int(height * zoom)), Image.Resampling.NEAREST
                ),
                (column * pw + 8, row * ph + 28),
            )
            draw.text((column * pw + 8, row * ph + 6), f"{name} / {kind}", fill="white")
        delta = abs(images["centre"].astype(int) - images["control"].astype(int))
        comparisons[kind] = {
            "centre_vs_control_changed_pixels": int((delta.max(2) > 0).sum()),
            "centre_vs_control_max_rgb8_error": int(delta.max()),
            "centre_vs_control_rgb8_rmse": float(np.sqrt((delta**2).mean())),
            "control_vs_original_changed_pixels": int(
                (images["control"] != images["original"]).any(2).sum()
            ),
        }
        error = Image.fromarray(np.minimum(delta * 4, 255).astype(np.uint8))
        contact.paste(
            error.resize(
                (int(width * zoom), int(height * zoom)), Image.Resampling.NEAREST
            ),
            (column * pw + 8, 3 * ph + 28),
        )
        draw.text((column * pw + 8, 3 * ph + 6), "4x centre-control", fill="white")
    contact.save(args.output / "comparison.png")
    np.savez_compressed(
        args.output / "raw.npz",
        **{
            name + "__" + key: value
            for name, fields in raw.items()
            for key, value in fields.items()
        },
    )
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "pose": "moving-023",
        "resolution": [view[3], view[4]],
        "roi": box.tolist(),
        "rays": len(origins),
        "bitwise_corner_fields_preserved": True,
        "geometry_corner_correspondence_preserved": True,
        "diagnostics": comparisons,
        "control_vs_original_fields": field_errors(raw["original"], raw["control"]),
        "centre_vs_control_fields": field_errors(raw["control"], raw["centre"]),
        "costs": costs,
        "native_known_field_array_bytes": {
            k: native[k].nbytes for k in ("positions", "normal", "uv", "metres")
        },
        "native_uint16_corner_bytes": native["indices"].size * 2,
        "diagnostic_expanded_array_bytes": {
            name: sum(v.nbytes for v in source.values())
            for name, source in sources.items()
        },
        "process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "Actual projected first-hit program/UV/metres and optional source-rule normal/colour diagnostics against stored native fields. Source-rule words use the original source frame before deformation; invalid weld faces expose a false validity flag and zero normal, magenta in diagnostics. No PBR, filtered material, cached-native triangulation/normal equivalence, C/T/S or device approval. Native/source support and triangulation differences remain visible, not normalized away. Per-corner transport is bitwise; screen interpolation can change when geometry moves.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
