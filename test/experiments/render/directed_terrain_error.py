"""Audit a directed point-projection bound against unchanged resting-object visibility."""

import argparse
import json
from pathlib import Path

import numpy as np
from current_visibility import project, raster
from terrain_grid_lod import stencil, triangles
from terrain_occluders import skirts
from terrain_projection_bound import row_sum_bound, spectral_bound


def directed_scale(rows, least, most, viewport, axis):
    result = np.zeros(len(least))
    clip_axis = rows @ axis
    for x in (least[:, 0], most[:, 0]):
        for y in (least[:, 1], most[:, 1]):
            delta = (clip_axis[:2] - np.column_stack((x, y)) * clip_axis[2]) * (
                0.5 * viewport
            )
            result = np.maximum(result, np.linalg.norm(delta, axis=1))
    return result + 64 * np.finfo(np.float32).eps * row_sum_bound(
        rows, least, most, viewport
    ) * np.linalg.norm(axis)


def split_projection(
    height_error, remainder, minimum_w, rows, least, most, viewport, axis
):
    h, r = np.broadcast_arrays(
        np.asarray(height_error, dtype=float), np.asarray(remainder, dtype=float)
    )
    if not all(
        np.isfinite(v).all() for v in (rows, least, most, viewport, axis)
    ) or np.any(least > most):
        return np.full_like(h, np.inf)
    valid = (
        np.isfinite(h)
        & np.isfinite(r)
        & (h >= 0)
        & (r >= 0)
        & np.isfinite(minimum_w[:, None])
        & (minimum_w[:, None] > 0)
    )
    h, r = np.where(valid, h, 0), np.where(valid, r, 0)
    scale = spectral_bound(rows, least, most, viewport)
    directed = directed_scale(rows, least, most, viewport, axis)
    denominator = (
        minimum_w[:, None] - h * abs(rows[2] @ axis) - r * np.linalg.norm(rows[2])
    )
    return np.divide(
        h * directed[:, None] + r * scale[:, None],
        denominator,
        out=np.full_like(h, np.inf, dtype=float),
        where=valid & (denominator > 0),
    )


def control():
    h = np.zeros((9, 9), dtype=float)
    h[2:4, 2:4] = -1 / 32
    y, x = np.indices(h.shape, dtype=float)
    top = np.stack((x, -y, h), axis=-1).reshape(-1, 3)
    vertices, extra, _ = skirts(top, 16, side=9)
    obj = np.array(
        [
            [x, y, z]
            for z in (-1 / 32, -1 / 64)
            for x, y in ((2.25, -2.75), (2.75, -2.75), (2.75, -2.25), (2.25, -2.25))
        ]
    )
    object_faces = np.array(
        [
            [4, 5, 6],
            [4, 6, 7],
            [0, 2, 1],
            [0, 3, 2],
            [0, 1, 5],
            [0, 5, 4],
            [1, 2, 6],
            [1, 6, 5],
            [2, 3, 7],
            [2, 7, 6],
            [3, 0, 4],
            [3, 4, 7],
        ]
    ) + len(vertices)
    vertices = np.vstack((vertices, obj))
    view = (
        np.array([4.0, -4.0, 20.0]),
        np.diag([1.0, 1.0, -1.0]),
        720 / (2 * np.tan(np.deg2rad(60) / 2)),
        1280,
        720,
    )
    return vertices, top, extra, object_faces, view


def residual_pairs(top, stride):
    if stride == 1:
        return top, top, len(triangles(9, 1))
    indices, weights, count = stencil(np.linspace(0, 1, 9), stride)
    fine = np.einsum("ijk,ij->ik", top[indices[:, :3]], weights[:, :3])
    coarse = np.einsum("ijk,ij->ik", top[indices[:, 3:]], -weights[:, 3:])
    return fine, coarse, count


def assess():
    vertices, top, extra, object_faces, view = control()
    low, high = top.min(axis=0), top.max(axis=0)
    eps = np.finfo(np.float32).eps
    cpu_margin = 32 * eps * (np.maximum(abs(low), abs(high)) + 1)
    a, b = (
        (low - cpu_margin).astype(np.float32).astype(float),
        (high + cpu_margin).astype(np.float32).astype(float),
    )
    shader_error = 8 * eps * (np.maximum(abs(a), abs(b)) + 1)
    low, high = a - shader_error, b + shader_error
    corners = np.array(
        [
            [x, y, z]
            for x in (low[0], high[0])
            for y in (low[1], high[1])
            for z in (low[2], high[2])
        ]
    )
    pp, zz = project(corners, view)
    ndc = np.column_stack((2 * pp[:, 0] / 1280 - 1, 1 - 2 * pp[:, 1] / 720))
    least, most = ndc.min(axis=0)[None], ndc.max(axis=0)[None]
    rows = view[1] * np.array([2 * view[2] / 1280, 2 * view[2] / 720, 1])[:, None]
    viewport, axis = np.array([1280, 720]), np.array([0.0, 0.0, 1.0])
    spectrum = spectral_bound(rows, least, most, viewport)[0]
    records = []
    for stride in (1, 2, 4, 8):
        fine, coarse, count = residual_pairs(top, stride)
        delta = coarse - fine
        eh, exy = np.abs(delta[:, 2]).max(), np.linalg.norm(delta[:, :2], axis=1).max()
        eh = float(np.nextafter(np.float32(eh), np.float32(np.inf)))
        remainder = exy + 2 * np.linalg.norm(shader_error)
        total = eh * np.linalg.norm(axis) + remainder
        original = total * spectrum / (zz.min() - total * np.linalg.norm(rows[2]))
        proposed = split_projection(
            np.array([[eh]]),
            np.array([[remainder]]),
            np.array([zz.min()]),
            rows,
            least,
            most,
            viewport,
            axis,
        ).item()
        pf, _ = project(fine, view)
        pc, _ = project(coarse, view)
        measured = np.linalg.norm(pf[:, :2] - pc[:, :2], axis=1).max()
        if stride == 1:
            original = proposed = 0.0
        records.append(
            {
                "stride": stride,
                "top_triangles": count,
                "height_bound_m": eh,
                "xy_residual_m": float(exy),
                "isotropic_rounding_m": float(remainder - exy),
                "isotropic_pixel_bound": float(original),
                "directed_pixel_bound": float(proposed),
                "measured_common_refinement_point_error_px": float(measured),
            }
        )
    old = max(i for i, r in enumerate(records) if r["isotropic_pixel_bound"] <= 0.5)
    new = max(i for i, r in enumerate(records) if r["directed_pixel_bound"] <= 0.5)
    projected, depth_z = project(vertices, view)
    if not (depth_z > 0.1).all():
        raise ValueError("reference requires no near-crossing source triangles")
    images, masks, depths = {}, {}, {}
    for name, level in (("reference", old), ("candidate", new)):
        terrain = np.vstack((triangles(9, records[level]["stride"]), extra))
        faces = np.vstack((terrain, object_faces))
        depths[name], owners = raster(
            projected, depth_z, faces, 1280, 720, with_faces=True
        )
        mask = owners >= len(terrain)
        masks[name] = mask
        image = np.zeros((720, 1280, 3))
        image[depths[name] > 0] = [0.4, 0.45, 0.35]
        image[mask] = [0.85, 0.18, 0.10]
        images[name] = image
    report = {
        "width": 1280,
        "height": 720,
        "pixel_allowance": 0.5,
        "levels": records,
        "old_selected_stride": records[old]["stride"],
        "directed_selected_stride": records[new]["stride"],
        "source_terrain_valley_m": -1 / 32,
        "object_bottom_m": -1 / 32,
        "object_top_m": -1 / 64,
        "reference_object_pixels": int(masks["reference"].sum()),
        "candidate_object_pixels": int(masks["candidate"].sum()),
        "lost_object_pixels": int((masks["reference"] & ~masks["candidate"]).sum()),
        "changed_material_pixels": int(
            np.any(images["reference"] != images["candidate"], axis=2).sum()
        ),
        "unchanged_outer_triangles": len(extra),
        "unchanged_object_triangles": len(object_faces),
        "scope": "Mathematical perspective control using existing 8-cell index patterns and complete common skirts/object. Ideal CPU raster with native-style retained margin formulas; no observed native bug or shader/compiler proof. Pixel point displacement alone does not admit scene depth/contact quality.",
    }
    return report, images, depths, masks


def display(report, images, masks, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(2, 3, figsize=(16, 8))
    y, x = np.nonzero(masks["reference"])
    for row in range(2):
        for axis, name in zip(axes[row, :2], ("reference", "candidate")):
            axis.imshow(images[name])
            axis.set(
                title=name + f" / object pixels {report[name + '_object_pixels']}",
                xlabel="pixel x",
                ylabel="pixel y",
            )
        axes[row, 2].imshow(
            masks["reference"] & ~masks["candidate"], cmap="magma", vmin=0, vmax=1
        )
        axes[row, 2].set(
            title="Lost resting-object pixels", xlabel="pixel x", ylabel="pixel y"
        )
        if row:
            for axis in axes[row]:
                axis.set(
                    xlim=(x.min() - 24, x.max() + 24), ylim=(y.max() + 24, y.min() - 24)
                )
    figure.suptitle(
        "Computed 1280×720 material-ID diagnostic; bottom row shares one object ROI / same 0.5 px point allowance"
    )
    figure.tight_layout()
    figure.savefig(output / "directed-terrain-contact.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    report, images, depths, masks = assess()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(
        args.output / "reference.npz",
        reference_depth=depths["reference"],
        candidate_depth=depths["candidate"],
        reference_object=masks["reference"],
        candidate_object=masks["candidate"],
    )
    if args.render:
        display(report, images, masks, args.output)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
