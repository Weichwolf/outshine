"""Full 2048-square reference on real shifts with declared analytic opaque casters."""

import argparse
import gzip
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from shadow_rebase_arithmetic import probe
from shadow_ring_reference import logical, pcf, pcf_scalar, raster, update
from shadow_translation_cost import FIXTURE, compare


def analytic_casters(matrix, side):
    """Fixed world triangles derived from the first real light projection, not a Place asset."""
    centres = [(0, 0), (side, side), (side, 0), (0, side)]
    centres += [(side * x / 4, side * y / 4) for x in range(1, 4) for y in range(1, 4)]
    screen = []
    for index, (x, y) in enumerate(centres):
        r, depth = side / 32, 0.2 + index / 32
        screen.append(
            [
                [x - r + 0.25, y - r + 0.25, depth],
                [x + r + 0.25, y - r + 0.25, depth],
                [x + 0.25, y + r + 0.25, depth],
            ]
        )
    clips = np.array(screen).reshape(-1, 3)
    clips[:, :2] = (clips[:, :2] / side - 0.5) * [2, -2]
    inverse = np.linalg.inv(np.asarray(matrix).reshape(4, 4, order="F"))
    world = np.c_[clips, np.ones(len(clips))] @ inverse.T
    return world[:, :3].reshape(-1, 3, 3)


def diagnostic(reference, ring, output):
    uv_axis = (np.arange(256) + 0.5) / 256
    uv = np.stack(np.meshgrid(uv_axis, uv_axis), axis=-1).reshape(-1, 2)
    receiver, slope = np.full(len(uv), 0.35), np.tile([0.001, -0.0015], (len(uv), 1))
    expected = pcf(ring, uv, receiver, slope)
    broken = pcf(ring, uv, receiver, slope, broken_physical_offsets=True)
    arrays = [
        reference,
        logical(ring),
        expected.reshape(256, 256),
        np.abs(expected - broken).reshape(256, 256),
    ]
    image = Image.new("RGB", (1024, 282), "white")
    labels = [
        "Full current depth",
        "Ring logical depth",
        "Logical PCF",
        "Wrong physical-slope PCF error",
    ]
    draw = ImageDraw.Draw(image)
    for index, (array, label) in enumerate(zip(arrays, labels, strict=True)):
        grey = Image.fromarray(np.uint8(np.clip(array, 0, 1) * 255)).resize((256, 256))
        image.paste(grey.convert("RGB"), (index * 256, 26))
        draw.text((index * 256 + 4, 6), label, fill="black")
    image.save(output / "depth-pcf.png")
    return int(np.count_nonzero(expected != broken))


def run(fixture, output):
    native_workload = compare(fixture)
    side = fixture["tile_px"]
    rows, final = [], None
    for region in range(4):
        triangles = analytic_casters(
            fixture["frames"][0]["world_matrices"][region], side
        )
        ring = copied = None
        for index, frame in enumerate(fixture["frames"]):
            matrix = frame["world_matrices"][region]
            work = {"triangle_pixel_candidates": 0, "covered_samples": 0}
            full = raster(triangles, matrix, side, counters=work)
            ring, ring_cost = update(ring, triangles, matrix, side)
            copied, copy_cost = update(copied, triangles, matrix, side, ring=False)
            np.testing.assert_array_equal(logical(ring), full)
            np.testing.assert_array_equal(logical(copied), full)
            # Independent scalar PCF checks include logical borders and each current physical seam.
            seam = (-np.asarray(ring.origin) % side) / side
            axis_x = [
                -0.1,
                0,
                0.5 / side,
                0.5,
                1 - 0.5 / side,
                1,
                1.1,
                seam[0] - 0.25 / side,
                seam[0] + 0.25 / side,
            ]
            axis_y = [
                -0.1,
                0,
                0.5 / side,
                0.5,
                1 - 0.5 / side,
                1,
                1.1,
                seam[1] - 0.25 / side,
                seam[1] + 0.25 / side,
            ]
            uv = np.stack(np.meshgrid(axis_x, axis_y), axis=-1).reshape(-1, 2)
            receiver = np.full(len(uv), 0.35)
            slopes = np.tile([0.001, -0.0015], (len(uv), 1))
            expected = [
                pcf_scalar(full, u, r, s)
                for u, r, s in zip(uv, receiver, slopes, strict=True)
            ]
            np.testing.assert_allclose(
                pcf(ring, uv, receiver, slopes), expected, rtol=0, atol=1e-15
            )
            rows.append(
                {
                    "region": region,
                    "frame": index,
                    "full": work,
                    "ring": ring_cost,
                    "copy": copy_cost,
                    "pcf_queries": len(uv),
                    "depth_differences": 0,
                    "pcf_max_error": float(
                        np.max(np.abs(pcf(ring, uv, receiver, slopes) - expected))
                    ),
                }
            )
            if region == 0:
                final = (full, ring)
        np.savez_compressed(
            output / f"region-{region}.npz",
            triangles=triangles,
            full=full,
            physical_ring=ring.data,
            ring_origin=ring.origin,
            copied=copied.data,
        )
    changed = diagnostic(*final, output)
    totals = {}
    for phase, selected in (
        ("initial", [r for r in rows if r["frame"] == 0]),
        ("subsequent", [r for r in rows if r["frame"] > 0]),
    ):
        totals[phase] = {
            kind: {
                key: sum(row[kind].get(key, 0) for row in selected)
                for key in (
                    "new_texels",
                    "triangle_pixel_candidates",
                    "covered_samples",
                    "staging_copy_bytes",
                    "retained_copy_bytes",
                )
            }
            for kind in ("full", "ring", "copy")
        }
    return {
        "states_including_initial": len(fixture["frames"]),
        "regions": 4,
        "tile_px": side,
        "analytic_triangles_per_region": 13,
        "depth_compared_texels": len(rows) * side * side,
        "pcf_queries": sum(r["pcf_queries"] for r in rows),
        "wrong_physical_slope_changed_diagnostic_queries": changed,
        "rows": rows,
        "totals": totals,
        "native_geometric_workload": native_workload,
        "rebase_arithmetic": probe(fixture),
        "native_caster_replay": False,
        "native_gpu_times_ms": None,
        "native_raster_equivalence": None,
        "allocation_count": None,
        "scope": "Float64 affine software raster with Float32 depth storage; ideal reuse only",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    started, cpu = time.perf_counter(), time.process_time()
    packed = args.input.read_bytes()
    result = run(json.loads(gzip.decompress(packed)), args.output)
    result["diagnostic_process"] = {
        "wall_s": time.perf_counter() - started,
        "cpu_s": time.process_time() - cpu,
        "peak_rss_bytes_linux": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "input_sha256": hashlib.sha256(packed).hexdigest(),
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in result.items() if k not in ("rows", "rebase_arithmetic")}
        )
    )


if __name__ == "__main__":
    main()
