"""Compare a source-first mass candidate with all original sources and remainder."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from compare_directed_groups import retained_subset, transformed
from compare_group_coverage import front_source, read, render
from compare_vertex_cluster_meshes import setup_view
from full_view_reference import load
from process_memory import peak_rss_bytes
from source_plane_mesh import factorized_positions
from triangle_coverage_reference import four_samples, integrate


def complete(product, original):
    if not np.array_equal(
        np.sort(np.r_[product["group_sources"], product["needs_sources"]]),
        np.unique(original["binding"][:, 1]),
    ):
        raise ValueError(
            "the original must cover every aggregated and remaining source"
        )
    rest = retained_subset(original, product["needs_sources"])
    candidate = factorized_positions(
        {
            "positions": np.vstack(
                (product["positions"], rest["positions"].astype(np.float32))
            ),
            "indices": np.vstack(
                (product["indices"], rest["indices"] + len(product["positions"]))
            ),
        }
    )
    control = factorized_positions(
        {
            "positions": original["positions"].astype(np.float32),
            "indices": original["indices"],
        }
    )
    return control, candidate, len(rest["indices"])


def coverage(sources, view, box):
    areas, samples, reports = {}, {}, {}
    for name, source in sources.items():
        wall, cpu = time.perf_counter(), time.process_time()
        oriented, orientation_bytes = front_source(source, view[0])
        areas[name], silhouette, work = integrate(oriented, view, box)
        samples[name] = four_samples(silhouette, areas[name].shape)
        reports[name] = {
            "source_vertices": len(source["positions"]),
            "source_triangles": len(source["indices"]),
            "geometry_bytes": source["positions"].nbytes + source["indices"].nbytes,
            "work": dict(work, orientation_coordinate_bytes=orientation_bytes),
            "coverage_sum_px2": float(areas[name].sum()),
            "integrated_l1_against_original_px2": float(
                abs(areas[name] - areas["original"]).sum()
            ),
            "maximum_pixel_coverage_error": float(
                abs(areas[name] - areas["original"]).max()
            ),
            "four_point_l1_against_original_px2": float(
                abs(samples[name] - samples["original"]).sum()
            ),
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
        }
    return areas, samples, reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    product = read(args.product)
    native, document, context, receipt = load(args.input)
    original = transformed(native, product["basis"], product["anchor"])
    control, candidate, retained_count = complete(product, original)
    sources = {"original": original, "control": control, "mass": candidate}
    frame = dict(
        product,
        source_bounds=np.array(
            [original["positions"].min(0), original["positions"].max(0)]
        ),
    )
    view, ground, box, _, _ = setup_view(
        {"positions": np.concatenate([s["positions"] for s in sources.values()])},
        frame,
        document,
        context,
        {"direction": [0.45, -0.5, 0.7]},
    )
    if (
        any(s["positions"][:, 2].min() <= ground for s in sources.values())
        or view[0][2] <= ground
    ):
        raise ValueError("isolated group must be above its unoccluding reference floor")
    areas, samples, reports = coverage(sources, view, box)
    metadata = sum(
        a.nbytes for k, a in product.items() if k not in ("positions", "indices")
    )
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_receipt": receipt,
        "product_sha256": hashlib.sha256(args.product.read_bytes()).hexdigest(),
        "view": "moving-023",
        "resolution": [int(view[3]), int(view[4])],
        "roi": box.tolist(),
        "reports": reports,
        "retained_original_sources": len(product["needs_sources"]),
        "retained_original_triangles": retained_count,
        "maximum_original_common_frame_float32_rounding_m": float(
            np.linalg.norm(
                original["positions"].astype(np.float32) - original["positions"], axis=1
            ).max()
        ),
        "candidate_metadata_bytes": metadata,
        "complete_candidate_geometry_and_metadata_bytes": metadata
        + reports["mass"]["geometry_bytes"],
        "process_peak_before_plot_bytes": peak_rss_bytes(),
        "scope": "One isolated front-facing opaque group/box-filter comparison with full unchanged-source remainder and a separately paid same-Float32 position-factorized original. No per-pixel source-ID requirement. GEOS Double area evaluation is not certified exact. All aggregate material/UV/metres/shading-normal fields are absent and unqualified; these position/index/metadata counts are not equal-quality full-asset or GPU savings. Added inter-building volumes may obscure unprovided transport/function gaps. No depth/caster/PBR/transition/native/device/world/p99 admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(
        args.output / "coverage.npz",
        **{k + "__area": a for k, a in areas.items()},
        **{k + "__sample": a for k, a in samples.items()},
    )
    np.savez(args.output / "complete-candidate.npz", **candidate)
    if args.render:
        from building_mass_view import construction

        render(areas, samples, args.output)
        construction(original, candidate, args.output)


if __name__ == "__main__":
    main()
