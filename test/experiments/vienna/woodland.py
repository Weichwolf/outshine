"""Place authored broadleaf recipes in source woodland on a camera-independent tile grid."""

import json
from dataclasses import replace
from functools import partial
from importlib.resources import files

import numpy as np
import shapely
from ground_surface import SOIL_MATERIALS, at_grade_width, route_area
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from park_avenue import avenue_sites
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
        first, inclination, reach = (0.18, 0.28), (0.52, 1.06), (3.25, 2.85)
        branching, twig_counts, radius_ratio = (7, 9), (14, 11), (0.85, 0.66)
    else:
        heights, radii, bends = (7.2, 8.1), (0.145, 0.23), (0.30, 0.21)
        first, inclination, reach = (0.20, 0.38), (0.50, 0.82), (3.0, 2.65)
        branching, twig_counts, radius_ratio = (8, 9), (12, 11), (0.86, 0.68)
    stem = replace(
        stem,
        length=heights[form],
        base_radius=radii[form],
        bend=bends[form],
        bend_variation=0.07,
    )
    limbs = replace(
        limbs,
        branches=branching[form],
        length=1.19 * reach[form],
        base_radius=0.22 if species == "oak" else 0.16,
        radius_parent_ratio=radius_ratio[form],
        first=first[form],
        attachment_jitter=0.49,
        angle=inclination[form],
        angle_tip=0.47 if form == 0 else 0.59,
        angle_variation=0.29,
        azimuth_variation=0.92,
        bend=0.29,
        bend_variation=0.35,
        tip_scale=0.58,
        length_shape="taper" if form == 0 else "rounded",
    )
    twigs = replace(
        twigs,
        branches=twig_counts[form],
        radius_parent_ratio=0.36,
        attachment_jitter=0.40,
        angle_variation=0.38,
        azimuth_variation=1.15,
        bend_variation=0.23,
        upward_bias=0.16,
        leaf_internode=0.018,
    )
    shoots = replace(
        twigs,
        branches=7,
        length=0.63 if species == "oak" else 0.57,
        first=0.16,
        last=0.92,
        base_radius=0.0022,
        tip_radius=0.00045,
        radius_parent_ratio=0.63,
        tip_scale=0.50,
        angle=1.1,
        angle_tip=0.62,
        attachment_jitter=0.42,
        angle_variation=0.36,
        upward_bias=0.20,
        azimuth_variation=1.23,
        leaf_internode=0.019,
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


def avenue_plan(form):
    beech = stand_plan("beech", form)
    stem, limbs, *outer = beech.levels
    stem = replace(stem, length=stem.length + 2.1, bend=stem.bend * 0.55)
    limbs = replace(limbs, first=0.43 + 0.06 * form, upward_bias=0.12)
    return PlantPlan(beech.seed + 821, (stem, limbs, *outer), beech.blades)


def represented_leaves(rows, areas, step):
    axes = rows["leaf_axis_ids"]
    order = np.arange(len(axes))
    if step == 1 or not len(order):
        return order, np.ones(len(order)), rows["leaf_origins"]
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
    # A fixed midpoint at even strides repeatedly selects the same side of a shoot.
    selected = order[first + np.random.default_rng(0).integers(stop - first)]
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
        if main_limb:
            rings, sides = (9, 10) if step <= 4 else (5, 8)
        elif step > 1:
            # These twigs have a radius below one sixth of the subpixel leaf width.
            rings, sides = 3, 3
        else:
            rings, sides = 4, 6
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
        if name in SOIL_MATERIALS and len(faces)
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
    lawn, avenue, avenue_marks = avenue_sites(source, radius, seed)
    if domain is None or domain.is_empty:
        positions, marks = np.empty((0, 2)), np.empty((0, 3))
    else:
        positions, marks = woodland_sites(source, domain, np.zeros(2), radius, seed)
    stand_count = len(positions)
    positions = np.concatenate((positions, avenue))
    marks = np.concatenate((marks, avenue_marks))
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
    for index, (xy, z, (family, rotation, scale), distance) in enumerate(
        zip(positions, heights, marks, distances, strict=True)
    ):
        managed = index >= stand_count
        species = "oak" if family < 0.5 and not managed else "beech"
        form = int(family * 4) % 2
        key = species, form, managed
        if key not in plans:
            plans[key] = avenue_plan(form) if managed else stand_plan(species, form)
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
                lawn if managed else root_domain,
                root_height,
                np.array([*xy, z - 0.08]),
                angle,
                scale,
                oak=species == "oak",
                seed=int(rotation * 2**32),
            )
            stems.add(f"{species} bark", stem["vertices"], stem["faces"])
        key = species, form, managed, group, near_stem
        templates.setdefault(key, []).append((*xy, z - 0.08, angle, scale))
    for (species, form, managed, group, near_stem), placements in templates.items():
        meshes = geometry(
            plans[species, form, managed],
            group,
            has_stem=not near_stem,
            oak=species == "oak",
        )
        yield (
            f"avenue {species}" if managed else species,
            group,
            meshes,
            np.asarray(placements),
        )
    if stems:
        yield "rooted stems", None, stems, np.array([[0, 0, 0, 0, 1]])
