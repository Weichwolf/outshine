"""Original Shell versus plan-first profiles; reference geometry is never a cook input."""

import argparse
import hashlib
import json
import resource
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from far_height_profiles import emit, plan_source
from far_product_cut import projection_error
from full_view_reference import load, views
from packed_normals import unpack
from parent_surface_inputs import open_parent

FIELDS = ("positions", "uv", "normal", "colour", "metres")


def subset(source, mask):
    selected = source["indices"][mask]
    ids, reverse = np.unique(selected, return_inverse=True)
    plan_binding = np.full((int(np.count_nonzero(mask)), 5), -1, np.int32)
    plan_binding[:, :3] = source["binding"][mask, 1:4]
    return {key: source[key][ids].copy() for key in FIELDS} | {
        "indices": reverse.astype(np.uint32).reshape(-1, 3),
        "source_id": source["binding"][mask, 1].astype(np.int32),
        "plan_binding": plan_binding,
        "native_binding": source["binding"][mask].copy(),
    }


def combine(parts):
    values = {
        key: []
        for key in (*FIELDS, "indices", "source_id", "plan_binding", "native_binding")
    }
    offset = 0
    for part in parts:
        for key in FIELDS:
            values[key].append(part[key])
        values["indices"].append(part["indices"] + offset)
        values["source_id"].append(part["source_id"])
        values["plan_binding"].append(part["plan_binding"])
        values["native_binding"].append(part["native_binding"])
        offset += len(part["positions"])
    return {key: np.concatenate(parts) for key, parts in values.items()}


def plan_cell(args, document):
    rows, needs, surfaces = {}, {}, {}
    with open_parent(args.input.parent / "original-inputs.tar.gz") as parent:
        cell, ordinals, forms = next(parent.iter_forms([document["view"]["cell"]]))
        expected = [s["index"] for s in document["sources"]]
        if list(ordinals) != expected:
            raise ValueError("Prepared forms and native source ordinals differ")
        started = time.perf_counter()
        for ordinal, surface in zip(ordinals, forms, strict=True):
            ordinal = int(ordinal)
            surfaces[ordinal] = surface
            try:
                rows[ordinal] = plan_source(ordinal, surface)
            except ValueError as error:
                needs[ordinal] = str(error)
        cost = time.perf_counter() - started
        identity = {
            int(i): parent.plans["records"][int(i)]["source_id"] for i in ordinals
        }
        return rows, needs, surfaces, identity, cost, parent.archive_sha256, cell


def selected_profiles(plans, surfaces, metadata, view):
    chosen, limits = [], {}
    eye, _, focal, width, height = view
    eye_box = np.stack((eye - 6, eye + 6))
    for ordinal, parts in plans.items():
        old_error = metadata[ordinal]["shell_error_m"]
        if old_error is None or old_error < 0:
            limits[ordinal] = None
            continue
        change = max(part.height_change_m for part in parts)
        frame = np.array([surfaces[ordinal][k] for k in ["east", "north", "up"]])
        bounds = np.array(surfaces[ordinal]["bounds"])
        magnitude = float(np.abs(bounds).max() + change + 1)
        rounding = np.sqrt(3) * (
            0.0005 * np.linalg.norm(frame, ord=2) + np.finfo(np.float32).eps * magnitude
        )
        displacement = float(
            np.nextafter(
                change * np.linalg.norm(frame[2]) + old_error + rounding, np.inf
            )
        )
        envelope = bounds + np.array([[-displacement], [displacement]])
        error = projection_error(
            envelope,
            eye_box,
            displacement_m=displacement,
            focal_px=focal,
            width=width,
            height=height,
        )
        limits[ordinal] = float(error)
        if error <= 1.0:
            chosen.append(ordinal)
    return set(chosen), limits


def write_scene(output, mesh, view, basis, centre, ground, producer):
    output.mkdir(parents=True, exist_ok=False)
    values = dict(mesh)
    values["positions"] = ((mesh["positions"].astype(float) - centre) @ basis.T).astype(
        np.float32
    )
    values["shading_normal"] = (unpack(mesh["normal"]) @ basis.T).astype(np.float32)
    np.savez_compressed(output / "mesh.npz", **values)
    eye, camera, focal, width, height = view
    scene = {
        "producer": producer,
        "mesh": "mesh.npz",
        "eye": ((eye - centre) @ basis.T).tolist(),
        "camera_basis": (camera @ basis.T).tolist(),
        "focal_px": focal,
        "width": width,
        "height": height,
        "ground": ground,
        "scope": "Isolated original-cell neutral opaque metallic-roughness control. Source vertex colour and normals retained; native procedural UV/PBR/alpha/environment/terrain not implemented or qualified.",
    }
    (output / "scene.json").write_text(json.dumps(scene, indent=2) + "\n")


def cook_selected(plans, required, needs):
    generated, bake_costs = {}, {}
    for ordinal in sorted(required):
        began = time.perf_counter()
        try:
            mesh = emit(plans[ordinal])
        except ValueError as error:
            needs[ordinal] = str(error)
            continue
        bake_costs[ordinal] = time.perf_counter() - began
        mesh["plan_binding"] = mesh.pop("profile_binding")
        mesh["source_id"] = mesh["plan_binding"][:, 0]
        mesh["native_binding"] = np.full((len(mesh["indices"]), 6), -1, np.int32)
        generated[ordinal] = mesh
    return generated, bake_costs


def comparison_views(source, document, context):
    all_views = views(document, context)
    named = {
        name: all_views[name] for name in ["static-020", "moving-020", "moving-021"]
    }
    midpoint = (
        source["positions"].min(0).astype(float) + source["positions"].max(0)
    ) / 2
    old_eye = named["static-020"][0]
    near_eye = (
        midpoint + (old_eye - midpoint) / np.linalg.norm(old_eye - midpoint) * 500
    )
    forward = (midpoint - near_eye) / np.linalg.norm(midpoint - near_eye)
    up = np.array(document["anchor_ecef"])
    up /= np.linalg.norm(up)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    near_basis = np.stack((right, np.cross(right, forward), forward))
    named["near-control"] = near_eye, near_basis, *named["static-020"][2:]
    return named, midpoint, right, up


def run(args):
    output = args.output.resolve()
    root = Path(__file__).resolve().parents[3]
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(root):
        raise ValueError("Private diagnostic output must use a fresh system-temp path")
    paths = [
        Path(__file__).resolve(),
        Path(__file__).with_name("far_height_profiles.py"),
    ]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, paths),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze profile sources before evidence")
    output.mkdir(parents=True, exist_ok=False)
    producer = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    source, document, context, receipt = load(args.input)
    if set(document["results"][0]["details"]) != {1}:
        raise ValueError("Complete original Shell control required")
    plans, needs, surfaces, identity, plan_seconds, archive_hash, cell = plan_cell(
        args, document
    )
    named, midpoint, right, up = comparison_views(source, document, context)
    metadata = {s["index"]: s for s in document["sources"]}
    choices = {
        name: selected_profiles(plans, surfaces, metadata, view)
        for name, view in named.items()
    }
    required = set().union(*(s for s, _ in choices.values()))
    generated, bake_costs = cook_selected(plans, required, needs)
    basis = np.stack((right, np.cross(up, right), up))
    local = (source["positions"].astype(float) - midpoint) @ basis.T
    ground = {
        "z": float(local[:, 2].min() - 1),
        "span": float(np.ptp(local[:, :2], axis=0).max() * 4),
    }
    rows = []
    for name, view in named.items():
        selected, limits = choices[name]
        selected &= set(generated)
        fallback = subset(source, ~np.isin(source["binding"][:, 1], list(selected)))
        candidate = combine([fallback, *(generated[i] for i in sorted(selected))])
        if set(candidate["source_id"]) != set(source["binding"][:, 1]):
            raise ValueError("Source disappeared from complete roundabout geometry")
        original = subset(source, np.ones(len(source["indices"]), bool))
        for strategy, mesh in [("original", original), ("profile", candidate)]:
            write_scene(
                output / name / strategy, mesh, view, basis, midpoint, ground, producer
            )
        rows.append(
            {
                "view": name,
                "selected_sources": sorted(selected),
                "screen_error_limits": limits,
                "original_triangles": len(original["indices"]),
                "profile_triangles": len(candidate["indices"]),
                "original_draw_array_bytes": sum(v.nbytes for v in original.values()),
                "profile_draw_array_bytes": sum(v.nbytes for v in candidate.values()),
            }
        )
    report = {
        "producer": producer,
        "source_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "original_archive_sha256": archive_hash,
        "reference_receipt": receipt,
        "cell": cell,
        "source_registry": [
            {"ordinal": i, "external_id": identity[i]} for i in sorted(identity)
        ],
        "planned_sources": len(plans),
        "needs": needs,
        "plan_seconds": plan_seconds,
        "generated_sources": len(generated),
        "bake_seconds": bake_costs,
        "views": rows,
        "process_raw_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "One original complete cell; same source coordinates, source IDs and footprint holes. Plan first, selection before profile triangulation, bake each required source once. All remaining source geometry retained. Provisional height/rounding displacement model, not a native Float32/PBR/radiance or transition certificate. Reference source geometry loaded for comparison/fallback only; no Fine/native generation.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "producer": producer,
                "sources": len(plans),
                "generated": len(generated),
                "needs": needs,
                "views": [
                    {
                        k: v
                        for k, v in r.items()
                        if k not in ["selected_sources", "screen_error_limits"]
                    }
                    for r in rows
                ],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
