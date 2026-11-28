"""Place authored broadleaf recipes in source woodland on a camera-independent tile grid."""

import json
from dataclasses import replace
from functools import partial
from importlib.resources import files

import numpy as np
import shapely
from ground_surface import SOIL_MATERIALS, at_grade_width, route_area
from montane_trees import SPRAY_WIDTH_M, conifer_sites, needle_sprays, spruce_plan
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from park_avenue import avenue_sites
from scene_mesh import Meshes, terrain_grid, terrain_height
from woodland_fork import joined_wood
from woodland_leaves import grouped_blades, leaf_group_ranges
from woodland_sites import woodland_sites
from woodland_stem import lean_tree, rooted_stem, scaffold_mesh, template_stem

COARSE_LEAF_STEP = 16
DISTANT_LEAF_STEP = 32


def stand_plan(species, form):
    data = files("outshine_ref.vegetation").joinpath("data", f"stand-{species}.json")
    base = PlantPlan.from_recipe(json.loads(data.read_text()))
    stem, limbs, twigs = base.levels
    if species == "oak":
        heights, radii, bends = (8.9, 10.8), (0.255, 0.15), (0.32, 0.26)
        first, inclination, reach = (0.15, 0.35), (0.48, 0.85), (3.25, 2.85)
        branching, twig_counts, radius_ratio = (7, 9), (14, 11), (0.85, 0.66)
    else:
        heights, radii, bends = (10.3, 12.4), (0.15, 0.19), (0.25, 0.15)
        first, inclination, reach = (0.22, 0.42), (0.40, 0.69), (3.0, 2.65)
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
        angle_tip=0.96 if species == "oak" else 0.84,
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
        length=twigs.length * (1.23 if species == "oak" else 1.14),
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
        branches=11 if species == "oak" else 13,
        length=0.53 if species == "oak" else 0.50,
        first=0.26,
        last=0.98,
        base_radius=0.0022,
        tip_radius=0.00045,
        radius_parent_ratio=0.63,
        tip_scale=0.62,
        angle=1.28,
        angle_tip=0.95,
        attachment_jitter=0.42,
        angle_variation=0.36,
        upward_bias=0.20,
        azimuth_variation=1.38,
        leaf_internode=0.023 if species == "oak" else 0.018,
        leaf_sky_alignment=0.49,
    )
    if form == 0:
        limbs = replace(
            limbs,
            first=0.23 if species == "oak" else 0.26,
            last=0.99,
            angle=1.15 if species == "oak" else 0.97,
            angle_tip=0.49,
        )
        twigs = replace(
            twigs,
            branches=twigs.branches - 3,
            first=0.16,
            last=0.96,
            tip_scale=0.55,
            angle=1.08,
            leaf_internode=0.016,
            upward_bias=0.14,
        )
        shoots = replace(
            shoots,
            branches=shoots.branches + 3,
            first=0.13,
            angle=1.34,
            angle_variation=0.47,
            length=shoots.length * 1.18,
            leaf_internode=shoots.leaf_internode * 1.07,
            length_shape="rounded",
            upward_bias=0.06,
        )
    return PlantPlan(base.seed + 19 * form, (stem, limbs, twigs, shoots), base.blades)


def habitat(source, *, stems=True, features=None):
    woods, excluded = [], []
    for layer, geom, props in source.features if features is None else features:
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
    broadleaf = stand_plan("oak", form)
    stem, limbs, *outer = broadleaf.levels
    stem = replace(
        stem,
        length=(8.6, 10.1)[form],
        base_radius=(0.35, 0.25)[form],
        bend=(0.23, 0.14)[form],
    )
    limbs = replace(
        limbs,
        length=(5.3, 4.5)[form],
        base_radius=(0.30, 0.23)[form],
        radius_parent_ratio=(0.84, 0.77)[form],
        first=(0.19, 0.35)[form],
        last=0.87,
        angle=(0.57, 0.81)[form],
        angle_tip=0.72,
        upward_bias=0.28,
    )
    return PlantPlan(broadleaf.seed + 821, (stem, limbs, *outer), broadleaf.blades)


def represented_leaves(rows, areas, step):
    order = np.arange(len(rows["leaf_axis_ids"]))
    if step == 1 or not len(order):
        return order, np.ones(len(order)), rows["leaf_origins"]
    order, first, stop = leaf_group_ranges(rows, step, merge_shoots=False)
    # A fixed midpoint at even strides repeatedly selects the same side of a shoot.
    selected = order[first + np.random.default_rng(0).integers(stop - first)]
    variants = rows["leaf_variants"]
    prefix = np.r_[0, np.cumsum(areas[variants[order]])]
    size = np.sqrt((prefix[stop] - prefix[first]) / areas[variants[selected]])
    return selected, size, rows["leaf_origins"][selected]


def crown_leaf_frames(rows, seed, *, oak):
    """Roll blades about their midribs, with flatter shade leaves lower in the crown."""
    frames = rows["leaf_frames"].copy()
    height = rows["leaf_origins"][:, 2]
    if not len(height):
        return frames
    crown_height = (height - height.min()) / max(np.ptp(height), 1e-6)
    roll_std = np.deg2rad(33 if oak else 19) * (0.65 + 0.70 * crown_height)
    roll = np.random.default_rng(seed).standard_normal(len(frames)) * roll_std
    roll = np.clip(roll, -1.25, 1.25)
    cosine, sine = np.cos(roll)[:, None], np.sin(roll)[:, None]
    side, normal = frames[:, :, 0].copy(), frames[:, :, 2].copy()
    frames[:, :, 0] = cosine * side - sine * normal
    frames[:, :, 2] = sine * side + cosine * normal
    return frames


def scaffold_parts(plan, step, species):
    rings, sides = (9, 10) if step <= 4 else (5, 8)
    if species == "spruce" and step >= COARSE_LEAF_STEP:
        rings, sides = 3, 5
    for path in plan.children(()):
        axis = plan.axis(path)
        part = scaffold_mesh(axis.length, axis.bend, *plan.radii(path), rings, sides)
        vertices = part["vertices"] @ axis.frame.T + axis.origin
        yield lean_tree(plan, vertices, oak=species == "oak"), part["faces"]


def geometry(plant, step, *, has_stem, species):
    spruce = species == "spruce"
    plan = plant.wood if spruce else plant
    rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
    meshes = Meshes()
    bark = f"{species} bark"
    oak = species == "oak"
    if has_stem:
        part = template_stem(plan, oak=oak)
        meshes.add(bark, part["vertices"], part["faces"])
    if has_stem or spruce:
        for vertices, faces in scaffold_parts(plan, step, species):
            meshes.add(bark, vertices, faces)
    for i in range(1, len(rows["axis_ids"])):
        main_limb = rows["axis_parent_ids"][i] == 0
        if main_limb or step >= COARSE_LEAF_STEP:
            continue
        shoot = rows["axis_paths"][i, 2] >= 0
        if step > 1 and shoot:
            continue
        if shoot:
            rings, sides = 3, 4
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
    if spruce:
        vertices, faces = needle_sprays(plant, step)
        meshes.add("spruce foliage", lean_tree(plan, vertices, oak=False), faces)
        return meshes
    variants = rows["leaf_variants"]
    areas = np.array([blade.area() for blade in plan.blades])
    frames = crown_leaf_frames(rows, plan.seed, oak=oak)
    if step >= COARSE_LEAF_STEP:
        vertices, faces = grouped_blades(
            rows,
            plan.blades,
            frames,
            areas,
            step,
            merge_shoots=step >= DISTANT_LEAF_STEP,
        )
        meshes.add("foliage", lean_tree(plan, vertices, oak=oak), faces)
        return meshes

    selected, size, origins = represented_leaves(rows, areas, step)
    frames = frames[selected]
    for variant, blade in enumerate(plan.blades):
        where = variants[selected] == variant
        if not where.any():
            continue
        part = blade.mesh(4 if step == 1 else 2)
        leaf_scale = size[where]
        if step > 1:
            a, b, c = part["vertices"][part["faces"]].transpose(1, 0, 2)
            coarse_area = np.linalg.norm(np.cross(b - a, c - a), axis=1).sum() / 2
            leaf_scale = leaf_scale * np.sqrt(areas[variant] / coarse_area)
        verts = (
            np.einsum("mj,nij->nmi", part["vertices"], frames[where])
            * leaf_scale[:, None, None]
            + origins[where, None, :]
        )
        faces = (
            part["faces"]
            + (len(part["vertices"]) * np.arange(len(verts)))[:, None, None]
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
    source,
    radius,
    seed,
    focal_pixels,
    root_domain,
    *,
    eye_xy=(0, 0),
    soil=None,
    eye_route=None,
    mature=None,
):
    domain = habitat(source)
    lawn, avenue, avenue_marks = avenue_sites(source, radius, seed)
    if domain is None or domain.is_empty:
        positions, marks = np.empty((0, 2)), np.empty((0, 3))
    else:
        positions, marks = woodland_sites(
            source, domain, np.zeros(2), radius, seed, mature=mature
        )
    stand_count = len(positions)
    positions = np.concatenate((positions, avenue))
    marks = np.concatenate((marks, avenue_marks))
    if not len(positions):
        return
    templates = {}
    plans = {}
    scaffolds = {}
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
    conifers = conifer_sites(
        source, positions[:stand_count], heights[:stand_count], marks[:stand_count, 0]
    )
    distances = (
        shapely.distance(shapely.points(positions), eye_route)
        if eye_route is not None
        else np.linalg.norm(positions - eye_xy, axis=1)
    )
    for index, (xy, z, (family, rotation, scale), distance) in enumerate(
        zip(positions, heights, marks, distances, strict=True)
    ):
        managed = index >= stand_count
        if managed:
            species = "chestnut"
        elif conifers[index]:
            species = "spruce"
        else:
            species = "oak" if family < 0.5 else "beech"
        form = int(family * 4) % 2
        key = species, form
        if key not in plans:
            if managed:
                plans[key] = avenue_plan(form)
            elif species == "spruce":
                plans[key] = spruce_plan(form)
            else:
                plans[key] = stand_plan(species, form)
        plant = plans[key]
        spruce = species == "spruce"
        plan = plant.wood if spruce else plant
        width = 0.30 * SPRAY_WIDTH_M if spruce else plan.blades[0].width
        width_px = width * scale * focal_pixels / max(1, distance)
        group = (
            1
            if width_px >= 1
            else (
                4
                if width_px >= 0.5
                else (COARSE_LEAF_STEP if width_px >= 0.30 else DISTANT_LEAF_STEP)
            )
        )
        angle = rotation * 2 * np.pi
        near_stem = distance < 32
        if near_stem:
            z = root_height(xy[None])[0]
            position = np.array([*xy, z - 0.08])
            stem = rooted_stem(
                plan,
                lawn if managed else root_domain,
                root_height,
                position,
                angle,
                scale,
                oak=species == "oak",
                seed=int(rotation * 2**32),
            )
            if spruce:
                wood = stem["vertices"], stem["faces"]
            else:
                scaffold = species, form, group
                if scaffold not in scaffolds:
                    scaffolds[scaffold] = tuple(scaffold_parts(plan, group, species))
                wood = joined_wood(
                    stem, scaffolds[scaffold], position, angle, scale, plan.radii(())[0]
                )
            stems.add(f"{species} bark", *wood)
        key = species, form, group, near_stem
        templates.setdefault(key, []).append((*xy, z - 0.08, angle, scale))
    for (species, form, group, near_stem), placements in templates.items():
        meshes = geometry(
            plans[species, form],
            group,
            has_stem=not near_stem,
            species=species,
        )
        yield (
            "avenue chestnut" if species == "chestnut" else species,
            group,
            meshes,
            np.asarray(placements),
        )
    if stems:
        yield "rooted stems", None, stems, np.array([[0, 0, 0, 0, 1]])
