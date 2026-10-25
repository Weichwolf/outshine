"""Finite opaque GGX mixture evaluation; no lighting baked into parent materials."""

import numpy as np
from material_filter import direct_ggx


def shade(
    albedo,
    weights,
    palette,
    normal,
    outgoing,
    light,
    tables,
    *,
    intensity=3,
    ambient=0.1,
):
    albedo, weights, normal, outgoing = [
        np.asarray(v, float) for v in (albedo, weights, normal, outgoing)
    ]
    if (
        albedo.ndim != 2
        or albedo.shape[1] != 3
        or normal.shape != albedo.shape
        or outgoing.shape != albedo.shape
        or weights.shape != (len(albedo), len(palette))
        or len(albedo) > 1048576
        or not all(np.isfinite(v).all() for v in (albedo, weights, normal, outgoing))
        or np.any(weights < 0)
        or np.any(abs(weights.sum(axis=1) - 1) > 8 * np.finfo(np.float32).eps)
    ):
        raise ValueError(
            "finite bounded opaque material measures and unit-mass weights required"
        )
    light, palette = np.array(light, float), np.asarray(palette, float)
    if (
        light.shape != (3,)
        or not np.isfinite(light).all()
        or np.linalg.norm(light) == 0
        or not np.isfinite(palette).all()
        or np.any((palette < 0) | (palette > 1))
        or np.any((albedo < 0) | (albedo > 1))
        or not np.isfinite([intensity, ambient]).all()
        or min(intensity, ambient) < 0
        or any(
            np.any(abs(np.linalg.norm(v, axis=1) - 1) > 1e-6)
            for v in (normal, outgoing)
        )
    ):
        raise ValueError(
            "physical reflectance, finite light and unit directions required"
        )
    light /= np.linalg.norm(light)
    result = ambient * albedo
    total, peak = 0, 0
    lit = (normal @ light > 0) & (np.einsum("ij,ij->i", normal, outgoing) > 0)
    for begin in range(0, len(albedo), 2048):
        local, component = np.nonzero(
            (weights[begin : begin + 2048] > 0) & lit[begin : begin + 2048, None]
        )
        if not len(local):
            continue
        owner = local + begin
        rows = np.zeros((len(owner), 20))
        rows[:, :3], rows[:, 3] = albedo[owner], palette[component]
        rows[:, 4:7], rows[:, 7] = 0.04, 1
        rows[:, 8:11], rows[:, 12:15], rows[:, 16:19] = (
            normal[owner],
            light,
            outgoing[owner],
        )
        value = direct_ggx(rows, tables)
        contribution = (value[:, :3] + value[:, 4:7]) * (
            intensity * weights[owner, component, None]
        )
        np.add.at(result, owner, contribution)
        total += len(owner)
        peak = max(peak, rows.nbytes)
    return result, {"ggx_lobe_evaluations": total, "maximum_row_array_bytes": peak}
