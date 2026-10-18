"""Full scalar counterexample, saved coefficients and actual geometry containment."""

import argparse
import hashlib
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from attached_crown import AttachedCrown
from attached_crown_build import cluster_recipe
from outshine_ref.vegetation import Blade, Shoot
from outshine_ref.vegetation.axis_surface import axis_mesh
from spatial_group_source import load_source


def row_digest(rows):
    return hashlib.sha256(
        b"".join(
            rows[k].tobytes()
            for k in ("ids", "variants", "origins", "frames", "nodes", "twig_ids")
        )
    ).hexdigest()


def check_attachments(form):
    data = form.data
    recipe = json.loads(form.recipe_json)
    t = data["parameters"]
    bend, length = recipe["carrier_bend"], recipe["carrier_length"]
    y = length * np.sin(bend * t) / bend if bend else length * t
    z = length * (1 - np.cos(bend * t)) / bend if bend else np.zeros(len(t))
    local = np.column_stack((np.zeros(len(t)), y, z))
    owner = np.repeat(
        np.arange(len(form.lower)), np.diff(data["host_offsets"]).astype(int)
    )
    expected = (
        np.einsum("nij,nj->ni", form.base.data["frames"][owner], local)
        + form.base.data["origins"][owner]
    )
    error = float(abs(expected - data["origins"]).max(initial=0))
    orthogonal = float(
        abs(data["frames"].transpose(0, 2, 1) @ data["frames"] - np.eye(3)).max(
            initial=0
        )
    )
    determinant = float(abs(np.linalg.det(data["frames"]) - 1).max(initial=0))
    if max(error, orthogonal, determinant) > 1e-13:
        raise AssertionError("independent physical attachment/frame check failed")
    return {
        "max_attachment_error_m": error,
        "max_frame_orthogonality_error": orthogonal,
        "max_frame_determinant_error": determinant,
    }


def check_geometry(source, form, rows):
    owners = np.repeat(
        np.arange(len(form.lower)), np.diff(form.base.data["offsets"]).astype(int)
    )
    maximum, vertices = -np.inf, 0
    for variant, blade in enumerate(source.blades):
        mesh = blade.mesh(8, 1)
        chosen = np.flatnonzero(rows["variants"] == variant)
        for begin in range(0, len(chosen), 512):
            ids = chosen[begin : begin + 512]
            points = np.einsum("nij,vj->nvi", rows["frames"][ids], mesh["vertices"])
            points += rows["origins"][ids, None]
            maximum = max(
                maximum,
                float((form.lower[owners[ids], None] - points).max()),
                float((points - form.upper[owners[ids], None]).max()),
            )
            vertices += len(ids) * len(mesh["vertices"])
    data = form.data
    twig_owner = np.repeat(
        np.arange(len(form.lower)), np.diff(data["host_offsets"]).astype(int)
    )
    for key in np.unique(data["keys"]):
        ids = np.flatnonzero(data["keys"] == key)
        for i in ids:
            mesh = axis_mesh(
                float(data[f"p{key}_length"]),
                0.35,
                float(data["radii"][i]),
                float(data["radii"][i]) * 0.3,
                3,
                4,
            )
            points = mesh["vertices"] @ data["frames"][i].T + data["origins"][i]
            maximum = max(
                maximum,
                float((form.lower[twig_owner[i]] - points).max()),
                float((points - form.upper[twig_owner[i]]).max()),
            )
            vertices += len(points)
    if maximum > 0:
        raise AssertionError("actual geometry outside conservative host bounds")
    return {
        "checked_geometry_vertices": vertices,
        "maximum_signed_bound_violation_m": maximum,
    }


def run(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    original = Shoot.detail
    prepared_counts = []

    def bounded(shoot, capacity):
        if capacity > 6:
            raise AssertionError("whole leaf preparation during compile")
        prepared_counts.append(capacity)
        return original(shoot, capacity)

    began = time.perf_counter()
    with (
        patch.object(Shoot, "detail", bounded),
        patch.object(Blade, "mesh", side_effect=AssertionError),
    ):
        form = AttachedCrown.build(source)
    if form is None:
        raise ValueError("source exceeds declared twig capacity")
    compile_s = time.perf_counter() - began
    form.save(output / "attached.npz")
    np.savez(
        output / "demand.npz",
        lower=form.lower,
        upper=form.upper,
        offsets=form.base.data["offsets"],
        source=np.asarray(source.identity),
    )
    old = cluster_recipe(source, form.base, 6)
    began = time.perf_counter()
    reference = old.tables(100000)
    scalar_s = time.perf_counter() - began
    np.savez(output / "full.npz", **reference)
    flat = output / "flat"
    flat.mkdir()
    for key, value in reference.items():
        np.save(flat / f"{key}.npy", value, allow_pickle=False)
    for mask in (
        np.ones(len(form.lower), bool),
        np.arange(len(form.lower)) % 2 == 0,
        np.zeros(len(form.lower), bool),
    ):
        actual = form.evaluate(mask, max_leaves=100000, anchors=True)
        owner = np.repeat(mask, np.diff(form.base.data["offsets"]).astype(int))
        for key in reference:
            np.testing.assert_array_equal(actual[key], reference[key][owner])
    complete = form.evaluate(
        np.ones(len(form.lower), bool), max_leaves=100000, anchors=True
    )
    np.testing.assert_array_equal(complete["ids"], np.arange(90000, dtype=np.uint64))
    np.testing.assert_array_equal(complete["variants"], source.crown.variants)
    np.testing.assert_allclose(
        complete["origins"] - source.crown.petiole_m * complete["frames"][:, :, 1],
        complete["nodes"],
        atol=3e-14,
        rtol=0,
    )
    geometry = check_geometry(source, form, complete)
    report = {
        "source_identity": source.identity,
        "identity": form.identity,
        "hosts": len(form.lower),
        "twigs": len(form.data["ids"]),
        "leaves": len(complete["ids"]),
        "prepared_local_prototypes": len(prepared_counts),
        "prepared_local_leaf_rows": sum(prepared_counts),
        "compile_s": compile_s,
        "scalar_full_s": scalar_s,
        "compact_array_bytes": sum(v.nbytes for v in form.data.values())
        + sum(v.nbytes for v in form.base.data.values()),
        "full_row_array_bytes": sum(v.nbytes for v in reference.values()),
        "compact_npz_bytes": (output / "attached.npz").stat().st_size,
        "full_npz_bytes": (output / "full.npz").stat().st_size,
        "pose_sha256": row_digest(complete),
        "whole_partial_empty_scalar_equal": True,
        "variant_counts": np.bincount(complete["variants"], minlength=8).tolist(),
        "analytic_blade_area_m2": sum(
            b.area() * n
            for b, n in zip(
                source.blades, np.bincount(complete["variants"]), strict=True
            )
        ),
        **geometry,
        **check_attachments(form),
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
