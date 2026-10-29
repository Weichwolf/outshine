"""Replay complete submitted geometry against conditional #990 box envelopes."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from itertools import product
from pathlib import Path

import numpy as np
from capture_box_envelope import box_envelope, conditionally_inside
from capture_box_reference import (
    ROOT,
    corner_envelope,
    submitted_inputs,
    typed_projection,
)
from capture_support_reference import metadata_inputs, original_parts
from vertex_projection.verify_envelope import rational_bound

DATA = ROOT / "test/outshine/data/render/building-field-composition"


def point_case(point, operands, envelope):
    model, view, shift = operands
    return {
        "model": model.ravel(order="F").astype(float).tolist(),
        "view": view.ravel(order="F").astype(float).tolist(),
        "shift": [*map(float, shift), 0.0],
        "point": [*map(float, point), 1.0],
        "clip": list(zip(envelope["lower"][0], envelope["upper"][0], strict=True)),
    }


def control(positions, operands):
    tiny = np.finfo(np.float32).tiny
    if (
        positions.dtype != np.float32
        or not np.isfinite(positions).all()
        or np.any((positions != 0) & (abs(positions) < tiny))
    ):
        raise ValueError("Complete positions must establish the family input domain")
    low, high = (
        positions.min(0, keepdims=True).astype(float),
        positions.max(0, keepdims=True).astype(float),
    )
    envelope = box_envelope(low, high, *operands, positions_normal_or_zero=True)
    corners = corner_envelope(low, high, *operands, positions_normal_or_zero=True)
    for point in [
        *product(*zip(low[0], high[0], strict=True)),
        positions[0],
        positions[len(positions) // 2],
        positions[-1],
    ]:
        rational_bound(point_case(point, operands, envelope))
    values = []
    for flat in (False, True):
        result = typed_projection(positions, *operands, flat=flat)
        if np.any((result < envelope["lower"]) | (result > envelope["upper"])):
            raise ValueError("Typed binary32 expression escaped its complete box")
        values.append(result)
    excess = np.maximum(
        corners["lower"] - envelope["lower"], envelope["upper"] - corners["upper"]
    )
    return (
        {
            "vertices": len(positions),
            "rational_corner_and_original_points": 11,
            "typed_lit_flat_positions": 2 * len(positions),
            "conditionally_inside": bool(conditionally_inside(envelope)[0]),
            "error_upper": envelope["error_upper"][0].tolist(),
            "excess_vs_eight_corners": excess[0].tolist(),
            "clip_lower": envelope["lower"][0].tolist(),
            "clip_upper": envelope["upper"][0].tolist(),
        },
        values[0],
        envelope,
    )


def plot_control(points, envelope, source_envelopes, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle

    fig, axes = plt.subplots(1, 3, figsize=(15, 5), layout="constrained")
    with np.load(DATA / "captures.npz", allow_pickle=False) as data:
        image = data["original_capture_colour"]
    axes[0].imshow(np.clip(image[..., :3], 0, 1) ** (1 / 2.2))
    axes[0].set_title("Frozen native original capture (display gamma only)")
    axes[0].axis("off")
    axes[1].scatter(
        points[:, 0] / points[:, 3],
        points[:, 1] / points[:, 3],
        s=0.2,
        c="0.3",
        rasterized=True,
    )
    low, high = envelope["lower"][0], envelope["upper"][0]
    axes[1].add_patch(
        Rectangle(
            low[:2],
            *(high - low)[:2],
            fill=False,
            edgecolor="darkorange",
            label="Conditional homogeneous x/y box",
        )
    )
    axes[1].add_patch(
        Rectangle(
            (-1, -1),
            2,
            2,
            fill=False,
            edgecolor="navy",
            ls="--",
            label="Capture x/y limits",
        )
    )
    axes[1].set(
        xlim=(-1.06, 1.06),
        ylim=(-1.06, 1.06),
        aspect="equal",
        title="All submitted vertices: CPU typed Lit control",
    )
    axes[1].legend(fontsize=7)
    radii = source_envelopes["error_upper"]
    for coordinate, label in enumerate(("x", "y", "reverse-Z", "w")):
        axes[2].plot(np.sort(radii[:, coordinate]), label=label)
    axes[2].set(
        yscale="log",
        xlabel="Source rank (each coordinate sorted)",
        ylabel="Conditional homogeneous error radius",
        title="1,891 complete source boxes; same #990 family",
    )
    axes[2].legend()
    fig.suptitle(
        "Numerical control only: production compiler, raster and World admission remain unqualified"
    )
    fig.savefig(output / "submitted-boxes.png", dpi=140)
    plt.close(fig)


def run(args):
    output = args.output.resolve()
    temp = Path(tempfile.gettempdir()).resolve()
    if output.is_relative_to(ROOT) or output == temp or not output.is_relative_to(temp):
        raise ValueError("Use a fresh explicit system-temp output")
    output.mkdir(parents=True, exist_ok=False)
    manifest, arrays, operands = submitted_inputs()
    metadata, headers = metadata_inputs(DATA)
    parts = original_parts(metadata)
    positions = np.concatenate([part["positions"] for part in parts])
    np.testing.assert_array_equal(positions, arrays["vertex_positions_f32"])
    offsets = np.cumsum([0] + [len(p["positions"]) for p in parts[:-1]])
    indices = np.concatenate(
        [p["indices"] + offset for p, offset in zip(parts, offsets, strict=True)]
    )
    np.testing.assert_array_equal(indices, arrays["triangle_indices_u32"])
    record, points, envelope = control(positions, operands)
    source_envelopes = box_envelope(
        headers["source_least_m"],
        headers["source_most_m"],
        *operands,
        positions_normal_or_zero=True,
    )
    # These source headers were qualified against all original attributes in #1720.
    # Repeat the actual per-triangle containment control against typed expressions.
    for part in parts:
        slots = np.searchsorted(headers["source_ids"], part["binding"][:, 1])
        if not np.array_equal(headers["source_ids"][slots], part["binding"][:, 1]):
            raise ValueError("Original source binding differs from metadata")
        for flat in (False, True):
            projected = typed_projection(part["positions"], *operands, flat=flat)[
                part["indices"]
            ]
            if np.any(projected < source_envelopes["lower"][slots, None]) or np.any(
                projected > source_envelopes["upper"][slots, None]
            ):
                raise ValueError(
                    "A complete original triangle escaped its source envelope"
                )
    legacy_path = (
        ROOT / "test/outshine/data/render/patch_selection/native-mesh-subset.json"
    )
    legacy = json.loads(legacy_path.read_text())
    view = np.asarray(legacy["mvp_f32"], np.float32).reshape(4, 4, order="F")
    shift = np.asarray(legacy["shift_f32"][:3], np.float32)
    records = []
    for mesh in legacy["meshes"]:
        model = np.asarray(mesh["model_f32"], np.float32).reshape(4, 4, order="F")
        row, _, _ = control(
            np.asarray(mesh["positions_f32"], np.float32), (model, view, shift)
        )
        records.append({"mesh_id": mesh["id"], **row})
    plot_control(points, envelope, source_envelopes, output)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "submitted_npz_sha256": manifest["npz_sha256"],
        "submitted_native_commit": manifest["source_commit"],
        "source_arrays_exact": True,
        "foreground": record,
        "complete_source_boxes": len(headers["source_ids"]),
        "conditionally_inside_source_boxes": int(
            conditionally_inside(source_envelopes).sum()
        ),
        "complete_triangles_checked_lit_and_flat": len(indices),
        "unresolved_sources_retained": metadata["source"]["unresolved_originals"],
        "legacy_fixture_sha256": hashlib.sha256(legacy_path.read_bytes()).hexdigest(),
        "legacy_complete_mesh_controls": records,
        "producer_expression_family_qualified": False,
        "raster_qualified": False,
        "world_qualified": False,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "complete_controls": 1 + len(records),
                "complete_source_boxes": len(headers["source_ids"]),
                "output": str(output),
                "world_qualified": False,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
