"""Place sedges and emergent reeds on natural ground next to solved water."""

from functools import cache

import numpy as np
import shapely
from outshine_ref.vegetation import Blade, Shoot
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from plant_sites import patch_density, root_elevations, scatter_mesh, tile_sites
from scene_mesh import Meshes

BANK_REACH_M = 40.0
WET_MARGIN_M = 5.5


@cache
def sedge():
    mesh = graminoid_tuft_mesh(2491, blades=14, segments=3)
    return {"bank sedge": (mesh["vertices"] * [1.6, 1.6, 0.93], mesh["faces"])}


@cache
def reed():
    length, bend = 1.27, 0.22
    culm = axis_mesh(length, bend, 0.0062, 0.0021, 5, 5)
    blade = Blade(
        length=0.37,
        width=0.033,
        lobes=0,
        widest=0.26,
        base_fill=0.37,
        bend=-0.66,
        fold=0.30,
        petiole=0,
    )
    shoot = Shoot(
        seed=91,
        length=length,
        bend=bend,
        internode=0.16,
        first=0.17,
        last=0.83,
        arrangement="distichous",
        leaf_angle=0.88,
        blade=blade,
    )
    leaves = shoot.detail(shoot.count)
    leaf = blade.mesh(3)
    meshes = Meshes()
    meshes.add("reed culms", culm["vertices"], culm["faces"])
    for origin, frame in zip(leaves["origins"], leaves["frames"], strict=True):
        meshes.add("reed leaves", leaf["vertices"] @ frame.T + origin, leaf["faces"])

    centres, tangent, lateral, normal = arc_frame(
        length, bend, 0, np.linspace(0.88, 1, 7)
    )
    for i, point in enumerate(centres):
        phase = i * 2.4
        radial = np.cos(phase) * lateral[i] + np.sin(phase) * normal[i]
        axis = 0.906 * tangent[i] + 0.423 * radial
        side = np.cross(radial, tangent[i])
        frame = np.stack((side, axis, np.cross(side, axis)), axis=1)
        plume = axis_mesh(0.12 - 0.009 * i, 0.10, 0.007 - 0.00055 * i, 0.0012, 3, 5)
        meshes.add(
            "reed seed heads", plume["vertices"] @ frame.T + point, plume["faces"]
        )
    stalk = {
        name: (np.asarray(vertices)[:, [0, 2, 1]] * [1, -1, 1], np.asarray(faces))
        for name, (vertices, faces) in meshes.items()
    }
    return {
        name: scatter_mesh(
            mesh, np.zeros((3, 3)), np.array([0, 2.4, 4.8]), np.array([1.0, 0.73, 0.89])
        )
        for name, mesh in stalk.items()
    }


def bank_habitat(source, shore, window):
    natural, wetland, managed = [], [], []
    for layer, shape, tags in source.features:
        if layer != "landcover" or not shape.intersects(window):
            continue
        cover, kind = tags.get("class"), tags.get("subclass")
        if cover == "wetland":
            wetland.append(shapely.make_valid(shape))
        elif cover == "wood" or (cover == "grass" and kind in {"grass", "meadow"}):
            natural.append(shapely.make_valid(shape))
        elif cover in {"grass", "sand"}:
            managed.append(shapely.make_valid(shape))
    mapped_wetland = shapely.union_all(wetland)
    habitat = shapely.union_all(natural).difference(shapely.union_all(managed))
    water = shapely.union_all([body.footprint for body in shore.bodies])
    open_ground = habitat.union(mapped_wetland).intersection(window)
    return open_ground.difference(
        water.union(shore.protection.footprint)
    ), mapped_wetland


def bank_plants(surfaces, source, shore, eye, seed):
    if not shore.contacts:
        return
    centre = eye[:2]
    window = shapely.Point(centre).buffer(BANK_REACH_M + 2)
    domain, mapped_wetland = bank_habitat(source, shore, window)
    if domain.is_empty:
        return
    sites = list(
        tile_sites(source, centre, BANK_REACH_M, seed ^ 589, density=6.8, cell_size=4)
    )
    if not sites:
        return
    xy = np.concatenate([points for points, _ in sites])
    marks = np.concatenate([values for _, values in sites])
    allowed = shapely.contains_xy(domain, *xy.T)
    xy, marks = xy[allowed], marks[allowed]
    index, owners = shore.coast_segments
    matches, near_water = index.query_nearest(
        shapely.points(xy),
        max_distance=WET_MARGIN_M,
        all_matches=False,
        return_distance=True,
    )
    points, contacts = matches
    if not len(points):
        return
    xy, marks = xy[points], marks[points]
    wetland = shapely.contains_xy(mapped_wetland, *xy.T)
    lake = np.zeros(len(xy), dtype=bool)
    level = np.empty(len(xy))
    body_ids = owners[contacts]
    for i in np.unique(body_ids):
        use = body_ids == i
        body = shore.bodies[i]
        level[use] = body.heights(source, xy[use])
        lake[use] = body.water_class == "lake" and body.control is None
    height = np.full(len(xy), -np.inf)
    for name in ("ground", "grass", "meadow"):
        if name in surfaces:
            np.maximum(height, root_elevations(surfaces[name], xy), out=height)
    above_water = height - level
    anchors = np.column_stack((xy, height - 0.014))
    patches = patch_density(source, xy, seed ^ 7831)
    fade = np.clip((BANK_REACH_M - np.linalg.norm(xy - centre, axis=1)) / 12, 0, 1)
    active = (
        (above_water >= 0.012)
        & (above_water < 0.92)
        & (marks[:, 2] < fade * (0.24 + 0.67 * patches))
    )
    tall = (
        (wetland | (lake & (patches > 0.54)))
        & (near_water < 2.8)
        & (above_water < 0.61)
    )
    angles = 2 * np.pi * marks[:, 0]
    scales = 0.62 + 0.75 * marks[:, 1] + 0.17 * patches
    clearance = shapely.distance(domain.boundary, shapely.points(xy))
    for plants, group in ((sedge, active & ~tall), (reed, active & tall)):
        if group.any():
            products = plants()
            radius = max(
                np.linalg.norm(v[:, :2], axis=1).max() for v, _ in products.values()
            )
            group &= clearance > scales * radius
            if group.any():
                for name, mesh in products.items():
                    yield (
                        name,
                        *scatter_mesh(
                            mesh, anchors[group], angles[group], scales[group]
                        ),
                    )
