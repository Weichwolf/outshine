"""Finite actual-leaf normal filtering using the existing correlated-Smith GGX."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from connected_material_fields import field_identity, material_contacts, validate_fields
from foliage_selection import directions
from leaf_sheet import average_blocks
from outshine_ref.vegetation.leaf_lighting import unit_directions
from outshine_ref.vegetation.leaf_normals import resolve_moments
from prepare_connected_materials import original_material
from relief_shading_reference import specular_from_dots


def response(normals, roughness, view, light):
    """Existing two-sided leaf-gloss orientation with per-texel roughness support."""
    normals = np.asarray(normals, float)
    shape = normals.shape[:-1]
    n = normals.reshape(-1, 3)
    if len(n) > 1048576:
        raise ValueError("bounded normal rows required")
    n, views, lights = (unit_directions(v, len(n)) for v in (n, view, light))
    n = n * np.where(np.sum(n * views, axis=1) < 0, -1, 1)[:, None]
    nl, nv = np.sum(n * lights, axis=1), np.sum(n * views, axis=1)
    active = (nl > 0) & (nv > 0)
    result = np.zeros(len(n))
    if np.any(active):
        half = views[active] + lights[active]
        half /= np.linalg.norm(half, axis=1)[:, None]
        nh = np.maximum(np.sum(n[active] * half, axis=1), 0)
        roughness = np.broadcast_to(roughness, shape).ravel()
        result[active] = specular_from_dots(
            roughness[active],
            nl[active],
            np.maximum(nv[active], 1e-6),
            nh,
            np.maximum(np.sum(views[active] * half, axis=1), 0),
        )
    return result.reshape(shape)


def compare(recipe, fields, colour, height, *, size, order):
    mask, slopes, _, _ = material_contacts(
        recipe, colour, height, size=size, order=order
    )
    normals = np.concatenate((slopes, np.ones((*mask.shape, 1))), axis=-1)
    normals /= np.linalg.norm(normals, axis=-1)[..., None]
    a, b = fields["slope_a"], fields["slope_b"]
    alpha = a[..., 3]
    np.testing.assert_array_equal(average_blocks(mask.astype(float), order), alpha)
    moments = np.stack(
        (a[..., 0], a[..., 1], a[..., 2], b[..., 0], b[..., 1], alpha * 0.55**4),
        axis=-1,
    )
    moments /= np.where(alpha > 0, alpha, 1)[..., None]
    normal, roughness, _ = resolve_moments(moments)
    rows = []
    for v, view in enumerate(directions(12)):
        for l, light in enumerate(directions(12)):
            reference = average_blocks(
                mask * response(normals, 0.55, view, light), order
            )
            for name, r in (("mean", 0.55), ("variance", roughness)):
                image = alpha * response(normal, r, view, light)
                error = abs(image - reference)
                rows.append(
                    {
                        "view": v,
                        "light": l,
                        "method": name,
                        "l1_over_coverage_mass": float(error.sum() / alpha.sum()),
                        "max_pixel_error": float(error.max()),
                    }
                )
    return rows


def study(original, carriers, fields, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    colour, height, identity, _ = original_material(original, carriers)
    product = json.loads((fields / "report.json").read_text())
    if product["material_identity"] != identity:
        raise ValueError("original material binding differs")
    start = time.perf_counter()
    cases = []
    for variant in range(8):
        path = fields / f"material-{variant}.json"
        descriptor = json.loads(path.read_text())
        with np.load(fields / f"material-{variant}.npz", allow_pickle=False) as z:
            arrays = dict(z)
        validate_fields(arrays)
        if field_identity(arrays, descriptor) != descriptor["identity"]:
            raise ValueError("finished material field content differs")
        rows = compare(
            descriptor["carrier_recipe"],
            arrays,
            colour,
            height,
            size=descriptor["map_size"],
            order=descriptor["midpoint_order"],
        )
        cases.append({"variant": variant, "comparisons": rows})
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cases": cases,
        "wall_s": time.perf_counter() - start,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Same finite captured original geometric normals,12 independent spherical views/lights, roughness.55, existing correlated-Smith single-scatter GGX with F0=.04. Per-UV-texel normal-filtering comparison; no bump variance, pigment coupling, visibility/area Jacobian or exact Blender Principled/IBL/whole-leaf optics oracle.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("original", "carriers", "fields", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    a = p.parse_args()
    study(a.original, a.carriers, a.fields, a.output)
