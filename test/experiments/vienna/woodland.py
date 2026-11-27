"""Place authored broadleaf recipes in source woodland on a camera-independent tile grid."""

import json
from importlib.resources import files

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area
from outshine_ref.places.sources import tile_geography
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.ground_cover import GroundCoverPlan
from scene_mesh import Meshes, terrain_grid, terrain_height


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


def geometry(plan, step):
    rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
    meshes = Meshes()
    for i in range(len(rows["axis_ids"])):
        part = axis_mesh(
            rows["axis_lengths"][i], rows["axis_bends"][i], *rows["axis_radii"][i], 4, 6
        )
        vertices = part["vertices"] @ rows["axis_frames"][i].T + rows["axis_origins"][i]
        meshes.add("bark", vertices, part["faces"])
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


def surface_roots(meshes, domain, terrain, xy, angle, stem_radius):
    stations = np.linspace(0.0, 1.0, 9)
    section = np.linspace(0, np.pi, 9)
    widths = stem_radius * (0.74 * (1.0 - stations) ** 1.4 + 0.04)
    heights = stem_radius * (
        0.96 * (1.0 - stations) ** 2.0 + 0.08 * np.sin(np.pi * stations)
    )
    ridges = (np.sin(section)[None, :] * heights[:, None] - 0.012).ravel()
    sides = len(section)
    cells = np.arange(len(stations) * sides).reshape(-1, sides)[:-1, :-1]
    faces = (cells[..., None] + [0, 1, sides, 1, sides + 1, sides]).reshape(-1, 3)
    for i, reach in enumerate((5.0, 6.6, 3.5, 4.3, 4.5)):
        bearing = angle + i * (2 * np.pi / 5) + 0.22 * np.sin(angle + i)
        direction = np.array([np.cos(bearing), np.sin(bearing)])
        across = np.array([-direction[1], direction[0]])
        curve = 0.72 * np.sin(bearing + i + 0.6) * np.sin(np.pi * stations)
        lateral = np.cos(section)[None, :] * widths[:, None]
        lateral += stem_radius * curve[:, None]
        for fraction in (1.0, 0.72, 0.48):
            line = xy + stations[:, None] * reach * stem_radius * fraction * direction
            points = (line[:, None, :] + lateral[:, :, None] * across).reshape(-1, 2)
            outline = shapely.MultiPoint(points).convex_hull
            if domain.covers(outline):
                z = terrain_height(terrain, points) + ridges
                meshes.add("bark", np.column_stack((points, z)), faces)
                break


def tree_batches(source, radius, seed, focal_pixels):
    domain = habitat(source)
    if domain is None or domain.is_empty:
        return
    templates = {}
    plans = {}
    root_mesh = Meshes()
    root_domain = habitat(source, stems=False)
    terrain, _ = terrain_grid(source, radius)

    for positions, marks in roots(source, domain, radius, seed):
        heights = source.height(positions)
        for xy, z, (family, rotation, scale) in zip(
            positions, heights, marks, strict=True
        ):
            species = "oak" if family < 0.5 else "beech"
            if species not in plans:
                data = (
                    files("outshine_ref.vegetation")
                    .joinpath("data", f"stand-{species}.json")
                    .read_text()
                )
                plans[species] = PlantPlan.from_recipe(json.loads(data))
            plan = plans[species]
            scale = 0.95 + scale * 0.95
            distance = np.linalg.norm(xy)
            width_px = plan.blades[0].width * scale * focal_pixels / max(1, distance)
            group = 1 if width_px >= 1 else (4 if width_px >= 0.5 else 16)
            angle = rotation * 2 * np.pi
            if distance < 32:
                z = terrain_height(terrain, xy[None])[0]
                surface_roots(
                    root_mesh,
                    root_domain,
                    terrain,
                    xy,
                    angle,
                    plan.levels[0].base_radius * scale,
                )
            key = species, group
            templates.setdefault(key, []).append((*xy, z - 0.08, angle, scale))
    for (species, group), placements in templates.items():
        yield species, group, geometry(plans[species], group), np.asarray(placements)
    if root_mesh:
        yield "surface roots", None, root_mesh, np.array([[0, 0, 0, 0, 1]])
