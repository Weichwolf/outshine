"""Existing physical needle generation plus actual dtype/attachment readers."""

import numpy as np
from outshine_ref.contracts import immutable
from outshine_ref.vegetation import NeedledPlant
from outshine_ref.vegetation.curve_frame import arc_frame
from recipe_graph_data import plain
from recipe_plant_values import PlantDetail, arrays
from spatial_crown_replay import check_mesh


def physical_detail(value):
    if not isinstance(value, PlantDetail):
        raise TypeError("Actual vegetation detail payload required")
    plan = NeedledPlant.from_recipe(plain(value.recipe))
    tables = value.tables
    for block in (tables, *value.needles):
        for a in block.values():
            if a.dtype.kind not in "iuf" or not np.isfinite(a).all():
                raise ValueError("Finite numeric physical buffers required")
            if a.dtype.kind == "f" and a.dtype != np.dtype("float64"):
                raise ValueError("Float64 port requires actual Float64 arrays")
    n, groups = sum(plan.needle_counts), sum(plan.group_counts)
    for key, count in (
        ("axis_ids", sum(plan.wood.axis_counts)),
        ("foliage_ids", n),
        ("foliage_short_ids", groups),
    ):
        if not np.array_equal(tables[key], np.arange(count, dtype=np.uint64)):
            raise ValueError("Complete canonical physical IDs required")
    for key in ("axis_frames", "foliage_frames", "foliage_short_frames"):
        frames = tables[key]
        if frames.shape[1:] != (3, 3) or (
            np.max(abs(frames.transpose(0, 2, 1) @ frames - np.eye(3))) > 2e-12
            or np.max(abs(np.linalg.det(frames) - 1)) > 2e-12
        ):
            raise ValueError("Proper rigid physical frames required")
    if np.any(tables["foliage_scales"] <= 0):
        raise ValueError("Positive needle scales required")
    errors = []
    for row, path in enumerate(plan.wood.paths()):
        pattern = plan.patterns[len(path)]
        mask = tables["foliage_short_axis_ids"] == tables["axis_ids"][row]
        if not mask.any():
            continue
        t = tables["foliage_parent_parameters"][mask]
        if pattern is None or np.any((t < pattern.first) | (t > pattern.last)):
            raise ValueError("Physical needle groups need a supporting axis interval")
        centre, tangent, _, _ = arc_frame(
            tables["axis_lengths"][row], tables["axis_bends"][row], 0, t
        )
        frame = tables["axis_frames"][row]
        radial = tables["foliage_parent_radials"][mask]
        radii = tables["axis_radii"][row]
        radius = radii[0] + (radii[1] - radii[0]) * t
        expected = (
            centre @ frame.T + tables["axis_origins"][row] + radius[:, None] * radial
        )
        errors.append(
            float(abs(expected - tables["foliage_short_origins"][mask]).max())
        )
        if np.max(abs(np.linalg.norm(radial, axis=1) - 1)) > 2e-12 or (
            np.max(abs(np.sum(radial * (tangent @ frame.T), axis=1))) > 2e-12
        ):
            raise ValueError("Twig attachment radial must be unit and perpendicular")
        selected = np.flatnonzero(mask)
        leaf = np.isin(tables["foliage_group_ids"], selected)
        group = tables["foliage_group_ids"][leaf].astype(int)
        anchor = (
            tables["foliage_short_origins"][group]
            + pattern.short_length * tables["foliage_short_frames"][group, :, 1]
        )
        errors.append(float(abs(anchor - tables["foliage_origins"][leaf]).max()))
    if not errors or max(errors) > 2e-12 or len(value.needles) != len(plan.needles):
        raise ValueError("Needle/short-shoot physical contacts disagree")
    for mesh in value.needles:
        check_mesh(mesh)
    return {
        "axes": sum(plan.wood.axis_counts),
        "needles": n,
        "groups": groups,
        "maximum_attachment_error_m": max(errors),
    }


def generate_detail(recipe, parameters):
    if set(parameters) != {"max_axes", "max_needles", "segments", "sides"}:
        raise ValueError("Explicit finite detail/work bounds required")
    a, n, segments, sides = (
        parameters[k] for k in ("max_axes", "max_needles", "segments", "sides")
    )
    if any(type(v) is not int for v in (a, n, segments, sides)) or not (
        1 <= a <= 512 and 1 <= n <= 16384 and 2 <= segments <= 16 and 4 <= sides <= 16
    ):
        raise ValueError("Pilot detail budget exceeded before physical generation")
    plan = NeedledPlant.from_recipe(plain(recipe))
    if sum(plan.wood.axis_counts) > a or sum(plan.needle_counts) > n:
        raise ValueError("Requested plant exceeds detail budget before placement")
    tables = plan.tables(a, n)
    value = PlantDetail(
        immutable(recipe),
        arrays(tables),
        tuple(arrays(needle.mesh(segments, sides)) for needle in plan.needles),
    )
    facts = physical_detail(value)
    return value, facts
