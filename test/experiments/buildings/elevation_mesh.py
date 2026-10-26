"""Partition existing wall surfaces, preserving exact cap/corner source vertices."""

import numpy as np
from elevation_cut import cut, walls
from facade_parts import append


def binding_for(mesh, mask, frame, bottom, binding, material):
    indices = mesh["faces"][mask].ravel()
    a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
    tangent = (b - a) / frame["width_m"]
    positions = mesh["vertices"][indices]
    uv = np.c_[(positions[:, :2] - a) @ tangent, positions[:, 2] - bottom]
    uv /= [frame["width_m"] / frame["columns"], frame["height_m"] / frame["rows"]]
    mesh["uv"][indices] = uv
    mesh["facade_extent"][indices] = [frame["columns"], frame["rows"], 0]
    identifier = binding(material, frame["fractions"])
    mesh["materials"][mask] = identifier
    return identifier


def compiled(mesh, source_frames, plans, binding, material):
    if not plans:
        return mesh, list(source_frames)
    chosen = {p.facade_id: p for p in plans}
    if len(chosen) != len(plans) or not chosen.keys() <= {
        f["id"] for f in source_frames
    }:
        raise ValueError("Each programme requires a distinct existing facade")
    bounds = (plans[0].scopes[0].base_m, *(s.top_m for s in plans[0].scopes))
    if any((p.scopes[0].base_m, *(s.top_m for s in p.scopes)) != bounds for p in plans):
        raise ValueError("Source elevations require common vertical cut planes")
    source, owners, remove, maximum = walls(mesh, source_frames, bounds[0])
    if maximum < bounds[-1]:
        raise ValueError("Actual wall does not cover logical elevation")
    levels = (*bounds, maximum) if maximum > bounds[-1] else bounds
    chunks = []
    frames = {}
    for number, (low, high) in enumerate(zip(levels, levels[1:])):
        clipped = cut(source, owners, low, high)
        if clipped is None:
            raise ValueError("Actual source has an empty vertical scope")
        part, labels = clipped
        xyz = part["vertices"][part["faces"]]
        for index, original in enumerate(source_frames):
            mask = labels == index
            if not mask.any():
                if number < 3:
                    raise ValueError("Actual facade is missing a required scope")
                continue
            p = chosen.get(original["id"])
            if number < 3:
                local = xyz[mask]
                area = (
                    np.linalg.norm(
                        np.cross(local[:, 1] - local[:, 0], local[:, 2] - local[:, 0]),
                        axis=1,
                    ).sum()
                    / 2
                )
                if abs(area - original["width_m"] * (high - low)) > 1e-7:
                    raise ValueError(
                        "Source does not cover the complete rectangular scope"
                    )
            if p and number >= 3:
                part["materials"][mask] = material
                continue
            if p:
                frame = p.frames()[number]
                identifier = binding_for(part, mask, frame, low, binding, material)
                frames[frame["id"]] = frame | {"scope_wall_material": identifier}
            else:
                binding_for(part, mask, original, bounds[0], binding, material)
                frames[original["id"]] = original
        chunks.append(part)
    retained = dict(
        mesh, faces=mesh["faces"][~remove], materials=mesh["materials"][~remove]
    )
    return append(retained, chunks), list(frames.values())
