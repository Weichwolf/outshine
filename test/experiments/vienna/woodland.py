"""Place authored broadleaf recipes in source woodland on a camera-independent tile grid."""

import json
from dataclasses import replace
from functools import partial
from importlib.resources import files

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from scene_mesh import Meshes, terrain_grid, terrain_height
from woodland_sites import woodland_sites
from woodland_stem import lean_tree, rooted_stem, template_stem

DISTANT_LEAF_STEP = 32


def stand_plan(species, form):
    data = files("outshine_ref.vegetation").joinpath("data", f"stand-{species}.json")
    base = PlantPlan.from_recipe(json.loads(data.read_text()))
    stem, limbs, twigs = base.levels
    if species == "oak":
        heights, radii, bends = (6.3, 7.4), (0.285, 0.18), (0.43, 0.30)
        first, inclination, reach = (0.19, 0.28), (0.57, 0.96), (3.0, 2.65)
    else:
        heights, radii, bends = (7.2, 8.1), (0.145, 0.23), (0.25, 0.19)
        first, inclination, reach = (0.30, 0.34), (0.81, 0.54), (2.55, 2.35)
    stem = replace(
        stem,
        length=heights[form],
        base_radius=radii[form],
        bend=bends[form],
        bend_variation=0.07,
    )
    limbs = replace(
        limbs,
        branches=8 if form == 0 else 9,
        length=reach[form],
        base_radius=0.175 if species == "oak" else 0.135,
        radius_parent_ratio=0.79 if species == "oak" and form == 0 else 0.73,
        first=first[form],
        attachment_jitter=0.46,
        angle=inclination[form],
        angle_tip=0.58,
        angle_variation=0.34,
        azimuth_variation=0.78,
        bend=0.43,
        bend_variation=0.23,
        tip_scale=0.62,
    )
    twigs = replace(twigs, branches=10 if form == 0 else 9, radius_parent_ratio=0.36)
    shoots = replace(
        twigs,
        branches=5,
        length=0.77 if species == "oak" else 0.65,
        first=0.24,
        last=0.92,
        base_radius=0.0022,
        tip_radius=0.00045,
        radius_parent_ratio=0.63,
        tip_scale=0.50,
        angle=1.1,
        angle_tip=0.62,
        upward_bias=0.27,
        azimuth_variation=0.85,
        leaf_internode=0.018,
        leaf_sky_alignment=0.49,
    )
    return PlantPlan(base.seed + 19 * form, (stem, limbs, twigs, shoots), base.blades)


def habitat(source, *, stems=True):
    woods, excluded = [], []
    for layer, geom, props in source.features:
        if layer == "landcover" and props.get("class") == "wood":
            woods.append(shapely.make_valid(geom))
        elif layer in ("building", "water"):
            excluded.append(shapely.make_valid(geom).buffer(0.6))
        elif layer == "transportation" and props.get("brunnel") != "tunnel":
            if not stems and (width := at_grade_width(props)) is not None:
                excluded.append(route_area(geom, width))
                continue
            margin = 1.4 if props.get("class") == "path" else 3.5
            excluded.append(
                geom.buffer(margin if geom.geom_type.endswith("LineString") else 0.6)
            )
    if not woods:
        return None
    return shapely.union_all(woods).difference(shapely.union_all(excluded))


def represented_leaves(rows, areas, step):
    axes = rows["leaf_axis_ids"]
    order = np.arange(len(axes))
    if not len(order):
        return order, np.empty(0), rows["leaf_origins"]
    if step >= DISTANT_LEAF_STEP:
        parents = rows["axis_parent_ids"][axes]
        axes = np.where(parents >= 0, parents, axes)
        order = np.argsort(axes, kind="stable")
    _, starts, counts = np.unique(axes[order], return_index=True, return_counts=True)
    first = np.concatenate(
        [np.arange(i, i + n, step) for i, n in zip(starts, counts, strict=True)]
    )
    ends = starts + counts
    stop = np.minimum(
        first + step, ends[np.searchsorted(starts, first, side="right") - 1]
    )
    selected = order[(first + stop - 1) // 2]
    variants = rows["leaf_variants"]
    prefix = np.r_[0, np.cumsum(areas[variants[order]])]
    size = np.sqrt((prefix[stop] - prefix[first]) / areas[variants[selected]])
    origins = rows["leaf_origins"][selected]
    if step >= DISTANT_LEAF_STEP:
        ordered = rows["leaf_origins"][order]
        sums = np.vstack((np.zeros(3), np.cumsum(ordered, axis=0)))
        origins = (sums[stop] - sums[first]) / (stop - first)[:, None]
    return selected, size, origins


def geometry(plan, step, *, has_stem, oak):
    rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
    meshes = Meshes()
    bark = "oak bark" if oak else "beech bark"
    if has_stem:
        part = template_stem(plan, oak=oak)
        meshes.add(bark, part["vertices"], part["faces"])
    for i in range(1, len(rows["axis_ids"])):
        if step > 1 and rows["axis_paths"][i, 2] >= 0:
            continue
        main_limb = rows["axis_parent_ids"][i] == 0
        if step >= 16 and not main_limb:
            continue
        rings, sides = (5, 8) if main_limb else (4, 6)
        if step > 1 and not main_limb:
            # These twigs have a radius below one sixth of the subpixel leaf width.
            rings, sides = 3, 3
        part = axis_mesh(
            rows["axis_lengths"][i],
            rows["axis_bends"][i],
            *rows["axis_radii"][i],
            rings,
            sides,
        )
        vertices = part["vertices"] @ rows["axis_frames"][i].T + rows["axis_origins"][i]
        meshes.add(bark, lean_tree(plan, vertices, oak=oak), part["faces"])
    # One representative spans the one-sided leaf area of its contiguous twig group.
    variants = rows["leaf_variants"]
    areas = np.array([blade.area() for blade in plan.blades])
    selected, size, origins = represented_leaves(rows, areas, step)
    for variant, blade in enumerate(plan.blades):
        where = variants[selected] == variant
        if not where.any():
            continue
        idx = selected[where]
        part = blade.mesh(4 if step == 1 else 2)
        leaf_scale = size[where]
        if step > 1:
            a, b, c = part["vertices"][part["faces"]].transpose(1, 0, 2)
            coarse_area = np.linalg.norm(np.cross(b - a, c - a), axis=1).sum() / 2
            leaf_scale = leaf_scale * np.sqrt(areas[variant] / coarse_area)
        verts = (
            np.einsum("mj,nij->nmi", part["vertices"], rows["leaf_frames"][idx])
            * leaf_scale[:, None, None]
            + origins[where, None, :]
        )
        faces = (
            part["faces"] + (len(part["vertices"]) * np.arange(len(idx)))[:, None, None]
        )
        meshes.add(
            "foliage",
            lean_tree(plan, verts.reshape(-1, 3), oak=oak),
            faces.reshape(-1, 3),
        )
    return meshes


def woodland_surface(surfaces):
    parts = [
        np.asarray(vertices)[np.asarray(faces, dtype=int)]
        for name, (vertices, faces) in surfaces.items()
        if name in {"ground", "grass", "meadow"} and len(faces)
    ]
    if not parts:
        return None
    triangles = np.concatenate(parts)
    area = np.linalg.det(triangles[:, 1:, :2] - triangles[:, :1, :2])
    triangles = triangles[abs(area) > 1e-12]
    if not len(triangles):
        return None
    vertices, indices = np.unique(triangles.reshape(-1, 3), axis=0, return_inverse=True)
    return SurfaceSampler(
        Mesh(vertices, indices.reshape(-1, 3)),
        xy_tolerance_m=2e-6,
        height_tolerance_m=3e-6,
        max_faces=len(triangles),
    )


def tree_batches(
    source, radius, seed, focal_pixels, root_domain, *, eye_xy=(0, 0), soil=None
):
    domain = habitat(source)
    if domain is None or domain.is_empty:
        return
    positions, marks = woodland_sites(source, domain, np.zeros(2), radius, seed)
    if not len(positions):
        return
    templates = {}
    plans = {}
    stems = Meshes()
    terrain, _ = terrain_grid(source, radius)
    sampler = woodland_surface(soil) if soil is not None else None

    def height(xy, fallback):
        if sampler is None:
            return fallback(xy)
        z = sampler.sample(xy).points_enu_m[:, 2].copy()
        missing = ~np.isfinite(z)
        if missing.any():
            z[missing] = fallback(xy[missing])
        return z

    root_height = partial(height, fallback=partial(terrain_height, terrain))

    heights = height(positions, source.height)
    distances = np.linalg.norm(positions - eye_xy, axis=1)
    for xy, z, (family, rotation, scale), distance in zip(
        positions, heights, marks, distances, strict=True
    ):
        species = "oak" if family < 0.5 else "beech"
        form = int(family * 4) % 2
        key = species, form
        if key not in plans:
            plans[key] = stand_plan(species, form)
        plan = plans[key]
        width_px = plan.blades[0].width * scale * focal_pixels / max(1, distance)
        group = (
            1
            if width_px >= 1
            else (
                4
                if width_px >= 0.5
                else (16 if width_px >= 0.30 else DISTANT_LEAF_STEP)
            )
        )
        angle = rotation * 2 * np.pi
        near_stem = distance < 32
        if near_stem:
            z = root_height(xy[None])[0]
            stem = rooted_stem(
                plan,
                root_domain,
                root_height,
                np.array([*xy, z - 0.08]),
                angle,
                scale,
                oak=species == "oak",
                seed=int(rotation * 2**32),
            )
            stems.add(f"{species} bark", stem["vertices"], stem["faces"])
        key = species, form, group, near_stem
        templates.setdefault(key, []).append((*xy, z - 0.08, angle, scale))
    for (species, form, group, near_stem), placements in templates.items():
        meshes = geometry(
            plans[species, form], group, has_stem=not near_stem, oak=species == "oak"
        )
        yield species, group, meshes, np.asarray(placements)
    if stems:
        yield "rooted stems", None, stems, np.array([[0, 0, 0, 0, 1]])
