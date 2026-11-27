"""Place authored broadleaf recipes in source woodland on a camera-independent tile grid."""

import json
from dataclasses import replace
from functools import partial
from importlib.resources import files

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area
from outshine_ref.places.sources import tile_geography
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.ground_cover import GroundCoverPlan
from scene_mesh import Meshes, terrain_grid, terrain_height
from woodland_stem import rooted_stem, template_stem


def stand_plan(species, form):
    data = files("outshine_ref.vegetation").joinpath("data", f"stand-{species}.json")
    base = PlantPlan.from_recipe(json.loads(data.read_text()))
    stem, limbs, twigs = base.levels
    if species == "oak":
        heights, radii, bends = (6.3, 7.4), (0.23, 0.17), (0.43, 0.30)
        first, inclination, reach = (0.20, 0.35), (0.71, 0.96), (3.0, 2.65)
    else:
        heights, radii, bends = (7.2, 8.1), (0.15, 0.19), (0.25, 0.19)
        first, inclination, reach = (0.30, 0.49), (0.81, 0.67), (2.55, 2.35)
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
        base_radius=0.105 if species == "oak" else 0.080,
        radius_parent_ratio=0.62,
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
    twigs = replace(twigs, radius_parent_ratio=0.36)
    return PlantPlan(base.seed + 19 * form, (stem, limbs, twigs), base.blades)


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


def roots(source, domain, radius, seed):
    camera = np.array([source.lon, source.lat])
    for receipt in source.receipts:
        if receipt["kind"] != "vector":
            continue
        tile = receipt["tile"]
        lat, lon = tile_geography([[0, 1], [1, 0]], 1, tile)
        sw, ne = np.column_stack((lon, lat))
        meters = np.array([111319.49 * np.cos(np.deg2rad(lat.mean())), 111132.0])
        lo = np.maximum(sw, camera - radius / source.metres)
        hi = np.minimum(ne, camera + radius / source.metres)
        if np.any(hi <= lo):
            continue
        plan = GroundCoverPlan(
            seed ^ (tile[1] << 24) ^ tile[2], density_m2=0.012, cell_m=16
        )
        sites = plan.sites((lo - sw) * meters, (hi - sw) * meters)
        xy = (sw - camera + sites["xy"] / meters) * source.metres
        keep = shapely.contains_xy(domain, *xy.T) & (
            np.linalg.norm(xy, axis=1) < radius
        )
        if keep.any():
            yield xy[keep], sites["marks"][keep]


def geometry(plan, step, *, has_stem, oak):
    rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
    meshes = Meshes()
    bark = "oak bark" if oak else "beech bark"
    if has_stem:
        part = template_stem(plan, oak=oak)
        meshes.add(bark, part["vertices"], part["faces"])
    for i in range(1, len(rows["axis_ids"])):
        sides = 8 if rows["axis_parent_ids"][i] == 0 else 6
        part = axis_mesh(
            rows["axis_lengths"][i],
            rows["axis_bends"][i],
            *rows["axis_radii"][i],
            5,
            sides,
        )
        vertices = part["vertices"] @ rows["axis_frames"][i].T + rows["axis_origins"][i]
        meshes.add(bark, vertices, part["faces"])
    # One representative spans the one-sided leaf area of its contiguous twig group.
    owners, starts, counts = np.unique(
        rows["leaf_axis_ids"], return_index=True, return_counts=True
    )
    first = np.concatenate(
        [np.arange(i, i + n, step) for i, n in zip(starts, counts, strict=True)]
    )
    stop = np.minimum(
        first + step,
        (starts + counts)[np.searchsorted(owners, rows["leaf_axis_ids"][first])],
    )
    selected = (first + stop - 1) // 2
    variants = rows["leaf_variants"]
    areas = np.array([blade.area() for blade in plan.blades])
    prefix = np.r_[0, np.cumsum(areas[variants])]
    size = np.sqrt((prefix[stop] - prefix[first]) / areas[variants[selected]])
    for variant, blade in enumerate(plan.blades):
        where = variants[selected] == variant
        if not where.any():
            continue
        idx = selected[where]
        part = blade.mesh(4)
        verts = (
            np.einsum("mj,nij->nmi", part["vertices"], rows["leaf_frames"][idx])
            * size[where, None, None]
            + rows["leaf_origins"][idx, None, :]
        )
        faces = (
            part["faces"] + (len(part["vertices"]) * np.arange(len(idx)))[:, None, None]
        )
        meshes.add("foliage", verts.reshape(-1, 3), faces.reshape(-1, 3))
    return meshes


def tree_batches(source, radius, seed, focal_pixels):
    domain = habitat(source)
    if domain is None or domain.is_empty:
        return
    templates = {}
    plans = {}
    stems = Meshes()
    root_domain = habitat(source, stems=False)
    terrain, _ = terrain_grid(source, radius)
    root_height = partial(terrain_height, terrain)

    for positions, marks in roots(source, domain, radius, seed):
        heights = source.height(positions)
        for xy, z, (family, rotation, scale) in zip(
            positions, heights, marks, strict=True
        ):
            species = "oak" if family < 0.5 else "beech"
            form = int(family * 4) % 2
            key = species, form
            if key not in plans:
                plans[key] = stand_plan(species, form)
            plan = plans[key]
            scale = 0.95 + scale * 0.95
            distance = np.linalg.norm(xy)
            width_px = plan.blades[0].width * scale * focal_pixels / max(1, distance)
            group = 1 if width_px >= 1 else (4 if width_px >= 0.5 else 16)
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
