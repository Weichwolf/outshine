"""Shared direct GGX/diffuse lighting for the analytic parent/control experiment."""

import numpy as np


def light_at(points, profile):
    if profile["kind"] == "point":
        delta = np.asarray(profile["position"]) - points
        distance = np.linalg.norm(delta, axis=1)
        return (
            delta / distance[:, None],
            (profile["intensity"] / np.maximum(distance, 1) ** 2)[:, None],
            distance,
        )
    direction = np.array(profile["direction"], float)
    direction /= np.linalg.norm(direction)
    return (
        np.broadcast_to(direction, points.shape),
        np.full((len(points), 1), profile["intensity"]),
        np.full(len(points), np.inf),
    )


def shade(points, normals, outgoing, albedo, visibility, profile):
    light, energy, _ = light_at(points, profile)
    normals, outgoing = np.broadcast_arrays(normals, outgoing)
    nl = np.maximum(np.einsum("ij,ij->i", normals, light), 0)
    nv = np.maximum(np.einsum("ij,ij->i", normals, outgoing), 0)
    halfway = light + outgoing
    halfway /= np.maximum(np.linalg.norm(halfway, axis=1)[:, None], 1e-15)
    nh = np.maximum(np.einsum("ij,ij->i", normals, halfway), 0)
    vh = np.maximum(np.einsum("ij,ij->i", outgoing, halfway), 0)
    a2 = 0.7**4
    distribution = a2 / (np.pi * (nh**2 * (a2 - 1) + 1) ** 2)
    g_light = 2 * nl / np.maximum(nl + np.sqrt(a2 + (1 - a2) * nl**2), 1e-15)
    g_view = 2 * nv / np.maximum(nv + np.sqrt(a2 + (1 - a2) * nv**2), 1e-15)
    fresnel = 0.04 + 0.96 * (1 - vh) ** 5
    specular = (
        distribution * g_light * g_view * fresnel / np.maximum(4 * nl * nv, 1e-15)
    )
    diffuse = (1 - fresnel) * albedo / np.pi
    direct = (diffuse + specular) * nl * visibility
    ambient = profile["ambient"]
    return np.broadcast_to(
        (direct[:, None] * energy + ambient * albedo), (len(points), 3)
    ).copy()


def display(linear):
    value = np.maximum(linear, 0)
    value = value / (1 + value)
    value = np.where(
        value <= 0.0031308, value * 12.92, 1.055 * value ** (1 / 2.4) - 0.055
    )
    return np.round(np.clip(value, 0, 1) * 255).astype(np.uint8)
