"""Joint substrate/litter quadrature; no displaced visibility or native sky model."""

import numpy as np
from relief_shading_reference import candidates, normals, roughened, specular
from terrain_material_plane import checked_at


def distribution(evaluate, at, dx, dy, order, *, derivative_step_m=1e-7):
    """Integrate point-material normals, not already filtered pixel normals."""
    frame = np.asarray((at, dx, dy), dtype=float)
    if (
        frame.shape != (3, 2)
        or not np.isfinite(frame).all()
        or abs(np.linalg.det(frame[1:])) <= 1e-30
        or type(order) is not int
        or not 2 <= order <= 256
        or not np.isfinite(derivative_step_m)
        or derivative_step_m <= 0
    ):
        raise ValueError("bounded nonsingular pixel frame and quadrature required")
    nodes, weights = np.polynomial.legendre.leggauss(order)
    x, y = np.meshgrid(nodes / 2, nodes / 2)
    at = checked_at(
        frame[0] + x.reshape(-1, 1) * frame[1] + y.reshape(-1, 1) * frame[2]
    )
    step = derivative_step_m
    raw = np.asarray(
        evaluate(
            at, np.tile([step, 0], (len(at), 1)), np.tile([0, step], (len(at), 1))
        ),
        dtype=float,
    ).copy()
    if raw.shape != (len(at), 6) or not np.isfinite(raw).all():
        raise ValueError("finite joint material samples required")
    raw[:, 4:] /= step
    return raw, (weights[:, None] * weights[None, :] / 4).ravel()


def response(raw, weights, lights, view):
    """Joint RGB/roughness/normal samples under the original guarded GGX model."""
    n = normals(raw[:, 4:])
    albedo, roughness = np.clip(raw[:, :3], 0, 1), np.clip(raw[:, 3], 0.04, 1)
    half = lights + view
    half /= np.linalg.norm(half, axis=-1, keepdims=True)
    fresnel = 0.04 + 0.96 * (1 - np.clip(half @ view, 0, 1)) ** 5
    return np.array(
        [
            weights
            @ (
                (1 - f) * albedo * np.maximum(n @ light, 0)[:, None] / np.pi
                + specular(n, roughness, light, view)[:, None]
            )
            for light, f in zip(lights, fresnel, strict=True)
        ]
    )


def moment_responses(raw, weights, lights, view):
    """Existing one-lobe candidates, explicitly not a GGX-convolution/LEAN proof."""
    mean = weights @ raw
    return {
        name: response(
            np.array([np.r_[mean[:3], roughness, -normal[:2] / normal[2]]]),
            np.ones(1),
            lights,
            view,
        )
        for name, (normal, roughness) in candidates(
            raw[:, 4:], weights, float(np.clip(mean[3], 0.04, 1))
        ).items()
    }


def joint_models(raw, weights):
    """Bounded light-independent PCA candidate; no exact GGX-filter guarantee."""
    n = normals(raw[:, 4:])
    roughness = np.clip(raw[:, 3], 0.04, 1)
    materials = np.column_stack((np.clip(raw[:, :3], 0, 1), roughness))
    features = np.column_stack((materials[:, :3], n, 2 * np.log(roughness)))

    def group(indices):
        local = weights[indices]
        area = local.sum()
        centred = features[indices] - local @ features[indices] / area
        values, vectors = np.linalg.eigh((centred.T * local) @ centred / area)
        return indices, area * max(values[-1], 0), vectors[:, -1]

    groups = [group(np.arange(len(raw)))]
    models = {}
    for capacity in (1, 2, 4, 8):
        while len(groups) < capacity:
            which = max(range(len(groups)), key=lambda i: groups[i][1])
            indices, score, axis = groups[which]
            if score <= 1e-20:
                break
            projected = features[indices] @ axis
            first = projected <= weights[indices] @ projected / weights[indices].sum()
            if np.all(first) or not np.any(first):
                break
            groups[which] = group(indices[first])
            groups.append(group(indices[~first]))
        fields, broadened, areas = [], [], []
        for indices, *_ in groups:
            area = weights[indices].sum()
            local = weights[indices] / area
            normal = local @ n[indices]
            length = min(float(np.linalg.norm(normal)), 1)
            material = local @ materials[indices]
            gradient = -normal[:2] / normal[2]
            fields.append(np.r_[material, gradient])
            broadened.append(
                np.r_[material[:3], roughened(float(material[3]), length), gradient]
            )
            areas.append(area)
        for name, representatives in (("mean", fields), ("toksvig", broadened)):
            models[f"joint_{capacity}_{name}"] = (
                np.array(representatives),
                np.array(areas),
            )
    return models
