"""Controlled material filtering on unchanged geometry, before any cache proposal.

Rows hold albedo/roughness, dielectric F0/F90, shading normal, light and view.
The CPU reference mirrors the documented isotropic opaque direct GGX model;
native shader evaluation is required separately. No Lambert substitution.
"""

import re

import numpy as np


def direction(degrees):
    angle = np.deg2rad(degrees)
    return np.stack((np.sin(angle), np.zeros_like(angle), np.cos(angle)), axis=-1)


def normalised(vectors):
    return vectors / np.linalg.norm(vectors, axis=-1, keepdims=True)


def filtered(a, b, weight, *, atlas=False, encoding=None):
    result = (1 - weight) * a + weight * b
    mean_normal = result[..., 8:11].copy()
    length = np.minimum(np.linalg.norm(mean_normal, axis=-1), 1)
    result[..., 8:11] = normalised(mean_normal)
    if atlas:
        a2 = (1 - weight) * a[..., 3] ** 4 + weight * b[..., 3] ** 4
        if encoding == "r":
            a2 = (np.floor(np.clip(a2, 0, 1) ** 0.25 * 255 + 0.5) / 255) ** 4
        elif encoding == "r4":
            a2 = np.floor(np.clip(a2, 0, 1) * 255 + 0.5) / 255
        elif encoding is not None:
            raise ValueError("unknown roughness encoding")
        # Existing roughenedBy/toksvigA2, after coverage-normalised r^4 mean.
        d = length * a2 + 2 * (1 - a2) * (1 - length)
        n = 2 * length * (1 - a2)
        result[..., 3] = (2 * d / (n + 2 * d)) ** 0.25
    return result


def fixtures():
    descriptions, batches = [], []
    for kind in ("albedo", "roughness", "normal", "constant_gloss", "native_pane"):
        for weight in (
            (0.5,)
            if kind in ("constant_gloss", "native_pane")
            else (0.0, 0.25, 0.5, 0.75, 1.0)
        ):
            for view in (0.0, 35.0, 65.0):
                angles = np.linspace(-80, 80, 161)
                a = np.zeros((len(angles), 20))
                a[:, :3], a[:, 3], a[:, 4:7], a[:, 7] = [0.4, 0.3, 0.2], 0.45, 0.04, 1
                a[:, 8:11], a[:, 12:15], a[:, 16:19] = (
                    [0, 0, 1],
                    direction(angles),
                    direction(view),
                )
                b = a.copy()
                if kind == "albedo":
                    a[:, :3], b[:, :3] = [0.18, 0.3, 0.5], [0.62, 0.28, 0.08]
                elif kind == "roughness":
                    a[:, 3], b[:, 3] = 0.2, 0.8
                elif kind == "normal":
                    a[:, 8:11], b[:, 8:11] = direction(-20.0), direction(20.0)
                    a[:, 3] = b[:, 3] = 0.3
                else:
                    a[:, 3] = b[:, 3] = 0.2
                    if kind == "native_pane":
                        # Opaque procedural pane values in facadeOpenings.glsl.
                        a[:, :3] = b[:, :3] = [0.025, 0.035, 0.045]
                        a[:, 3] = b[:, 3] = 0.12
                a, b = (
                    a.astype(np.float32).astype(float),
                    b.astype(np.float32).astype(float),
                )
                mean, atlas = filtered(a, b, weight), filtered(a, b, weight, atlas=True)
                byte = filtered(a, b, weight, atlas=True, encoding="r4")
                perceptual = filtered(a, b, weight, atlas=True, encoding="r")
                # Endpoint controls must use precisely the same represented inputs.
                if weight in (0, 1):
                    mean = atlas = (a if weight == 0 else b).copy()
                batches.append(np.stack((a, b, mean, atlas, byte, perceptual), axis=1))
                descriptions.append(
                    {"kind": kind, "weight": weight, "view_degrees": view}
                )
    return np.stack(batches).astype(np.float32), descriptions


def read_tables(path):
    text = path.read_text()
    tables = {}
    for name in ("kGgxAlbedo", "kGgxFresnelBias", "kGgxAlbedoAverage"):
        match = re.search(
            r"const float " + name + r"\[\] = float\[\]\((.*?)\);", text, re.DOTALL
        )
        if match is None:
            raise ValueError("missing native GGX table: " + name)
        tables[name] = np.fromstring(match[1], sep=",", dtype=np.float32).astype(float)
    roughness = int(re.search(r"kEnergyRoughnessSteps = (\d+)", text)[1])
    view = int(re.search(r"kEnergyViewSteps = (\d+)", text)[1])
    if (
        tables["kGgxAlbedo"].size != roughness * view
        or tables["kGgxAlbedoAverage"].size != roughness
    ):
        raise ValueError("inconsistent native table dimensions")
    tables["shape"] = roughness, view
    return tables


def energy_scale(tables, roughness, nv, f0):
    nr, nv_steps = tables["shape"]
    rf, vf = np.clip(roughness, 0, 1) * (nr - 1), np.clip(nv, 0, 1) * (nv_steps - 1)
    r0, v0 = rf.astype(int), vf.astype(int)
    r1, v1 = np.minimum(r0 + 1, nr - 1), np.minimum(v0 + 1, nv_steps - 1)
    rt, vt = rf - r0, vf - v0
    grid = tables["kGgxAlbedo"].reshape(nr, nv_steps)
    e = (1 - rt) * ((1 - vt) * grid[r0, v0] + vt * grid[r0, v1]) + rt * (
        (1 - vt) * grid[r1, v0] + vt * grid[r1, v1]
    )
    avg = tables["kGgxAlbedoAverage"]
    average = (1 - rt) * avg[r0] + rt * avg[r1]
    favg = f0 + (1 - f0) / 21
    multiple = favg * average[:, None] / (1 - favg * (1 - average[:, None]))
    missing = np.divide(1 - e, e, out=np.zeros_like(e), where=(e > 0) & (e < 1))
    return 1 + multiple * missing[:, None]


def direct_ggx(rows, tables):
    rows = np.asarray(rows, dtype=float).reshape(-1, 20)
    albedo, roughness, f0, f90 = rows[:, :3], rows[:, 3], rows[:, 4:7], rows[:, 7]
    n, light, view = [normalised(rows[:, start : start + 3]) for start in (8, 12, 16)]
    half = normalised(light + view)
    nl = np.maximum(np.einsum("ij,ij->i", n, light), 0)
    nv = np.maximum(np.einsum("ij,ij->i", n, view), 1e-6)
    nh = np.maximum(np.einsum("ij,ij->i", n, half), 0)
    vh = np.maximum(np.einsum("ij,ij->i", view, half), 0)
    a2 = roughness**4
    distribution = np.divide(
        a2, np.pi * (nh * nh * (a2 - 1) + 1) ** 2, out=np.zeros_like(a2), where=a2 > 0
    )
    denominator = nl * np.sqrt(nv * nv * (1 - a2) + a2) + nv * np.sqrt(
        nl * nl * (1 - a2) + a2
    )
    visibility = np.divide(
        0.5, denominator, out=np.zeros_like(denominator), where=denominator > 0
    )
    fresnel = f0 + (f90[:, None] - f0) * (1 - vh[:, None]) ** 5
    diffuse = (1 - fresnel) * albedo / np.pi
    specular = (
        fresnel
        * (distribution * visibility)[:, None]
        * energy_scale(tables, roughness, nv, f0)
    )
    return np.column_stack((diffuse * nl[:, None], nl, specular * nl[:, None], nv))
