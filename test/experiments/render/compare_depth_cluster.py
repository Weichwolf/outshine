"""Depth-cluster image counterprobe on complete original native cell controls."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_native_cell_views import load_pair, low_eye_view
from compare_source_billboard import depth_difference, difference, panel
from depth_cluster import bake, fields, prepare, reference
from native_source_mesh import face_sources
from raster_depth_cluster import raster
from source_billboard import average_samples, image_rays, projected_bounds
from source_far_reprojection import prepare_source
from source_surface_fields import relight


def material_error(depths, fine_faces, slots, actual, cached, cache, owners):
    both = np.isfinite(depths[0]) & np.isfinite(depths[-1])
    ids = np.flatnonzero(both)
    cf = cache["trace"]["face"][slots[ids]]
    same_source = owners[fine_faces[ids]] == owners[cf]
    same_code = actual["code"][ids] == cached["code"][ids]
    same_domain = actual["domain"][ids] == cached["domain"][ids]
    selected = ids[same_source & same_code & same_domain]
    angle = np.degrees(
        np.arccos(
            np.clip((actual["normal"][ids] * cached["normal"][ids]).sum(1), -1, 1)
        )
    )
    result = {
        "common_samples": len(ids),
        "source_changed_samples": int((~same_source).sum()),
        "material_code_changed_samples": int((~same_code).sum()),
        "normal_over_1_degree_samples": int((angle > 1).sum()),
        "same_source_code_domain_samples": len(selected),
        "scope": "Raw shader-input differences, not native PBR image errors; UV units follow the original native streams.",
    }
    for key in ("uv", "metres"):
        if key == "metres" and not cache["has_metres"]:
            result[key + "_p95_l2"] = None
            result[key + "_max_l2"] = None
            continue
        subset = selected if key == "uv" else selected[actual["domain"][selected] == 0]
        delta = np.linalg.norm(actual[key][subset] - cached[key][subset], axis=1)
        result[key + "_p95_l2"] = (
            float(np.percentile(delta, 95)) if len(delta) else None
        )
        result[key + "_max_l2"] = float(delta.max()) if len(delta) else None
    return result


def render(sources, indices, cache, prepared, view, suns, owners):
    bounds = [s["positions"] for s in sources] + [
        prepared["corners"][prepared["valid"]].reshape(-1, 3)
    ]
    boxes = [projected_bounds(p, view) for p in bounds if len(p)]
    lo = np.maximum(np.floor(np.min(boxes, axis=(0, 1))).astype(int), 0)
    hi = np.minimum(np.ceil(np.max(boxes, axis=(0, 1))).astype(int), view[3:])
    width, height = hi - lo
    if width <= 0 or height <= 0:
        raise ValueError("empty comparison is not quality evidence")
    origins, directions = image_rays(view, np.array([lo, hi]), width, height)
    depths, attributes, faces, times = [], [], [], []
    for source, index in zip(sources, indices, strict=True):
        start = time.perf_counter()
        depth, face, values, _ = reference(source, index, origins, directions)
        times.append(time.perf_counter() - start)
        depths.append(depth)
        attributes.append(values)
        faces.append(face)
    start = time.perf_counter()
    depth, slots, work = raster(cache, prepared, view, lo, width, height)
    raster_s = time.perf_counter() - start
    depths.append(depth)
    start = time.perf_counter()
    attributes.append(fields(cache, slots))
    gather_s = time.perf_counter() - start
    titles = (
        "Fine",
        "Shell selection",
        *(("Massed selection",) if len(sources) == 3 else ()),
        "Depth cache",
    )
    images, errors = [], []
    lighting_s = []
    for sun in suns:
        start = time.perf_counter()
        light = {
            title: average_samples(
                np.column_stack((relight(f, sun, d), np.isfinite(d))), width, height, 2
            )
            for title, d, f in zip(titles, depths, attributes, strict=True)
        }
        errors.append(
            {
                title: difference(light["Fine"], image)
                for title, image in light.items()
                if title != "Fine"
            }
        )
        images.append(light)
        lighting_s.append(time.perf_counter() - start)
    fine_hit, cache_hit = np.isfinite(depths[0]), np.isfinite(depths[-1])
    return images, {
        "roi": [lo.tolist(), hi.tolist()],
        "query_subpixels": len(origins),
        "fine_covered_subpixels": int(fine_hit.sum()),
        "raw_missing_subpixels": int((fine_hit & ~cache_hit).sum()),
        "raw_extra_subpixels": int((~fine_hit & cache_hit).sum()),
        "depth": depth_difference(depths[0], depths[-1], cache_hit),
        "material_inputs": material_error(
            depths, faces[0], slots, attributes[0], attributes[-1], cache, owners
        ),
        "lights": errors,
        "work": work,
        "reference_query_s": times,
        "cache_raster_s": raster_s,
        "cache_visible_material_gather_s": gather_s,
        "visible_material_samples": int((slots >= 0).sum()),
        "selected_material_operand_bytes": int((slots >= 0).sum()) * 27,
        "lighting_all_methods_s": lighting_s,
    }


def save_cache(cache, directory):
    directory.mkdir()
    np.save(directory / "runtime.npy", cache["data"], allow_pickle=False)
    np.save(directory / "trace.npy", cache["trace"], allow_pickle=False)
    eye, basis, focal, width, height = cache["view"]
    descriptor = {
        "schema": 1,
        "scope": "Diagnostic single front depth; unvalidated footprint, no PBR or opaque coverage admission.",
        "eye": eye.tolist(),
        "basis": basis.tolist(),
        "focal_px": focal,
        "image_width_height": [width, height],
        "lo": cache["lo"].tolist(),
        "capture_width_height": [cache["width"], cache["height"]],
        "supersample": 2,
        "has_metres": cache["has_metres"],
        "density": cache["density"],
        "membership": {
            k: cache["membership"][k]
            for k in ("original_parent", "original_cell", "original_sources")
        },
    }
    data = (json.dumps(descriptor, separators=(",", ":")) + "\n").encode()
    (directory / "descriptor.json").write_bytes(data)
    return {
        "records": len(cache["data"]),
        "runtime_record_bytes": cache["data"].nbytes,
        "trace_record_bytes": cache["trace"].nbytes,
        "descriptor_bytes": len(data),
        "serialized_total_bytes": sum(p.stat().st_size for p in directory.iterdir()),
    }


def view_at_eye(positions, eye, up, focal):
    centre = (positions.min(0).astype(float) + positions.max(0)) / 2
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    return (
        eye,
        np.stack((right, np.cross(right, forward), forward)),
        focal,
        1280,
        720,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fine", "shell", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mass", type=Path)
    parser.add_argument("--geometry-only-shell", action="store_true")
    parser.add_argument("--native-eye", action="store_true")
    parser.add_argument("--adaptive", action="store_true")
    parser.add_argument("--scales", type=float, nargs="+", default=[32, 128])
    parser.add_argument("--yaws", type=float, nargs="+", default=[0, 44, 90])
    args = parser.parse_args()
    if not all(np.isfinite(s) and 1 <= s <= 1024 for s in args.scales) or not all(
        np.isfinite(y) for y in args.yaws
    ):
        raise ValueError("finite bounded capture poses required")
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    pair, native, _ = load_pair(
        args.fine, args.shell, allow_mixed_codes=args.geometry_only_shell
    )
    sources = list(pair)
    if args.mass:
        mass, _, _ = load_pair(args.fine, args.mass)
        sources.append(mass[1])
    owners = face_sources(args.fine, sources[0])
    out.mkdir(parents=True, exist_ok=False)
    up = np.array(native[0]["anchor_ecef"])
    up /= np.linalg.norm(up)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    focal = native[0]["view"]["focal_px"]
    suns = [0.3 * east - 0.4 * north + 0.8 * up, -0.6 * east + 0.3 * north + 0.5 * up]
    start = time.perf_counter()
    indices = [prepare_source(s) for s in sources]
    index_s = time.perf_counter() - start
    captures, records = [], []
    for scale in [None] if args.native_eye else args.scales:
        for yaw in [None] if args.native_eye else args.yaws:
            capture_view = (
                view_at_eye(
                    sources[0]["positions"],
                    np.array(native[0]["view"]["eye_ecef"]) - native[0]["anchor_ecef"],
                    up,
                    focal,
                )
                if args.native_eye
                else low_eye_view(
                    sources[0]["positions"], up, east, north, focal, scale, yaw, 0
                )
            )
            start = time.perf_counter()
            cache = bake(sources[0], indices[0], capture_view, adaptive=args.adaptive)
            bake_s = time.perf_counter() - start
            start = time.perf_counter()
            prepared = prepare(cache)
            prepare_s = time.perf_counter() - start
            tag = "native-eye" if args.native_eye else f"{scale}-{yaw}"
            cost = save_cache(cache, out / ("capture-" + tag))
            sampled = np.unique(owners[cache["trace"]["face"]]).tolist()
            captures.append(
                {
                    "tag": tag,
                    "cost": cost,
                    "bake_s": bake_s,
                    "prepare_patches_s": prepare_s,
                    "prepared_patch_array_bytes": prepared["array_bytes"],
                    "capture_rays": cache["source_rays"],
                    "front_capture_samples": cache["front_capture_samples"],
                    "material_records_gathered": cache["material_records_gathered"],
                    "merged_full_pixels": cache["merged_full_pixels"],
                    "sampled_local_sources": sampled,
                    "unsampled_local_sources": sorted(
                        set(range(len(native[0]["sources"]))) - set(sampled)
                    ),
                    "sample_binding_max_residual_m": cache[
                        "sample_binding_max_residual_m"
                    ],
                    "depth_rounding_max_m": cache["depth_rounding_max_m"],
                }
            )
            poses = [
                (
                    f"move-{move}",
                    (capture_view[0] + move * capture_view[1][0], *capture_view[1:]),
                )
                for move in (0, -1, 1, -6, 6)
            ]
            basis = capture_view[1].copy()
            angle = np.radians(10)
            basis[0] = (
                np.cos(angle) * capture_view[1][0] - np.sin(angle) * capture_view[1][2]
            )
            basis[2] = (
                np.sin(angle) * capture_view[1][0] + np.cos(angle) * capture_view[1][2]
            )
            poses.append(("look-10", (capture_view[0], basis, *capture_view[2:])))
            if args.native_eye:
                # This is a deliberate large positional change, not a turn in place.
                centre = (
                    sources[0]["positions"].min(0).astype(float)
                    + sources[0]["positions"].max(0)
                ) / 2
                eye = 2 * centre - capture_view[0]
                opposite = view_at_eye(sources[0]["positions"], eye, up, focal)
            else:
                opposite = low_eye_view(
                    sources[0]["positions"],
                    up,
                    east,
                    north,
                    focal,
                    scale,
                    yaw + 180,
                    0,
                )
            poses.append(("opposite", opposite))
            for name, view in poses:
                images, record = render(
                    sources, indices, cache, prepared, view, suns, owners
                )
                name = tag + "-" + name
                panel(out / (name + ".png"), images, name)
                np.savez_compressed(
                    out / (name + ".npz"),
                    **{
                        f"{i}-{k}": v
                        for i, light in enumerate(images)
                        for k, v in light.items()
                    },
                )
                record.update(
                    tag=name,
                    eye=view[0].tolist(),
                    basis=view[1].tolist(),
                    eye_displacement_m=float(np.linalg.norm(view[0] - capture_view[0])),
                )
                records.append(record)
                print(
                    json.dumps(
                        {
                            "case": name,
                            "missing": record["raw_missing_subpixels"],
                            "extra": record["raw_extra_subpixels"],
                        }
                    ),
                    flush=True,
                )
    report = {
        "scope": "Complete original cell. 1280x720/original focal/2x2 AA; one front sample per capture ray; current diagnostic Lambert/colour factor, not native PBR/world/terrain/shadow/device acceptance. Raw metrics before Fine fallback. Sample bindings do not certify extended supports.",
        "source": sources[0]["metadata"],
        "native_eye_capture": args.native_eye,
        "focal_px": focal,
        "suns": [s.tolist() for s in suns],
        "native_triangles": [len(s["indices"]) for s in sources],
        "native_bytes": [s["native_bytes"] for s in sources],
        "shell_unknown_program_triangles": int((~sources[1]["code_uniform"]).sum()),
        "prepare_reference_indices_s": index_s,
        "captures": captures,
        "cases": records,
        "peak_process_rss_linux_kib": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "cost_scope": "40B packed runtime record, 16B per-sample provenance, actual JSON/NumPy headers and transient patch arrays counted. All selected cache samples scanned/projected; two triangle equivalents per supported front sample. No native GPU/layout/residency claim; library allocations/traffic incomplete. Bake, prepare, query separate. Native bytes are Raised payload, not full comparable artifact size.",
        "fallback": "All products unvalidated. Opposite-view case lies outside short-reuse domain and tests missing surfaces. Whole-cell Fine fallback/readiness required; retaining Fine plus cache costs at least both payloads before GPU/allocator/roundabout/mip/shadow overhead. No oracle per-pixel fallback or free cache-refresh claimed.",
        "input_sha256": {
            str(p / name): hashlib.sha256((p / name).read_bytes()).hexdigest()
            for p in ([args.fine, args.shell] + ([args.mass] if args.mass else []))
            for name in (
                "input.json",
                "native.json",
                "mesh/receipt.json",
                "subset.index",
                "subset.forms",
            )
        },
        "code_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in (
                Path(__file__).name,
                "depth_cluster.py",
                "raster_depth_cluster.py",
                "source_far_assets.py",
                "source_far_reprojection.py",
                "source_surface_fields.py",
                "source_billboard.py",
                "compare_source_billboard.py",
                "compare_native_cell_views.py",
                "packed_normals.py",
            )
        },
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
