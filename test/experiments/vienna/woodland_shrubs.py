"""Place low woody growth between the forest floor and mature crowns."""

from functools import cache

import numpy as np
import shapely
from outshine_ref.vegetation import AxisLevel, Blade, PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from plant_sites import root_elevations, tile_sites
from scene_mesh import Meshes

SHRUB_REACH_M = 42.0


def shrub_plan(sapling, form):
    stem = AxisLevel(
        branches=1,
        length=(1.77 + 0.34 * form) if sapling else 0.31,
        base_radius=0.031,
        tip_radius=0.007,
        bend=0.12 + 0.07 * form,
    )
    limbs = AxisLevel(
        branches=7 if sapling else 9,
        length=0.65 if sapling else 1.03 + 0.16 * form,
        base_radius=0.011,
        tip_radius=0.0024,
        radius_parent_ratio=0.58,
        first=0.30 if sapling else 0.10,
        last=0.96 if sapling else 0.73,
        tip_scale=0.39 if sapling else 0.56,
        length_shape="taper" if sapling else "rounded",
        angle=0.91 if sapling else 1.05,
        angle_tip=0.49,
        angle_variation=0.17,
        azimuth_variation=0.45,
        bend=0.28,
        upward_bias=0.24,
        leaf_internode=0.091,
        arrangement="distichous",
    )
    shoots = AxisLevel(
        branches=7,
        length=0.33 if sapling else 0.43,
        base_radius=0.0023,
        tip_radius=0.00048,
        radius_parent_ratio=0.62,
        first=0.22,
        last=0.95,
        length_shape="rounded",
        angle=1.09,
        bend=0.30,
        leaf_internode=0.031 if sapling else 0.033,
        leaf_sky_alignment=0.55,
        arrangement="distichous",
    )
    leaf = Blade(
        length=0.095 if sapling else 0.109,
        width=0.054 if sapling else 0.072,
        lobes=0,
        widest=0.42,
        fold=0.24,
        petiole=0.006,
    )
    return PlantPlan(9633 + 11 * sapling + form, (stem, limbs, shoots), (leaf,))


@cache
def prototype(sapling, form):
    plan = shrub_plan(sapling, form)
    rows = plan.tables(sum(plan.axis_counts), sum(plan.leaf_counts))
    meshes = Meshes()
    for index, origin in enumerate(rows["axis_origins"]):
        length, bend = rows["axis_lengths"][index], rows["axis_bends"][index]
        segments = 6 if index == 0 else 3
        part = axis_mesh(length, bend, *rows["axis_radii"][index], segments, 5)
        vertices = part["vertices"] @ rows["axis_frames"][index].T + origin
        meshes.add("shrub wood", vertices, part["faces"])
    leaf = plan.blades[0].mesh(3)
    vertices = (
        np.einsum("mj,nij->nmi", leaf["vertices"], rows["leaf_frames"])
        + rows["leaf_origins"][:, None, :]
    )
    offsets = np.arange(len(vertices)) * len(leaf["vertices"])
    meshes.add(
        "sapling leaves" if sapling else "shrub leaves",
        vertices.reshape(-1, 3),
        (leaf["faces"] + offsets[:, None, None]).reshape(-1, 3),
    )
    products = {name: tuple(map(np.asarray, mesh)) for name, mesh in meshes.items()}
    reach = max(np.linalg.norm(v[:, :2], axis=1).max() for v, _ in products.values())
    return products, reach


def woodland_shrubs(mesh, source, domain, eye, seed):
    if domain is None or domain.is_empty:
        return
    centre = eye[:2]
    window = shapely.Point(centre).buffer(SHRUB_REACH_M + 3)
    habitat = domain.intersection(window)
    if habitat.is_empty:
        return
    sites = list(
        tile_sites(
            source, centre, SHRUB_REACH_M, seed ^ 8453, density=0.37, cell_size=8
        )
    )
    if not sites:
        return
    xy = np.concatenate([b[0] for b in sites])
    marks = np.concatenate([b[1] for b in sites])
    lon, lat = (np.array([source.lon, source.lat]) + xy / source.metres).T
    patches = np.sin(12300 * lon + 7800 * lat) * np.cos(6300 * lon - 19400 * lat)
    distance = np.linalg.norm(xy - centre, axis=1)
    fade = np.clip((SHRUB_REACH_M - distance) / 10, 0, 1)
    active = (marks[:, 2] < fade * (0.24 + 0.61 * np.maximum(0, patches))) & (
        shapely.contains_xy(habitat, *xy.T)
    )
    xy, marks = xy[active], marks[active]
    elevations = root_elevations(mesh, xy)
    anchors = np.column_stack((xy, elevations - 0.025))
    clearance = shapely.distance(habitat.boundary, shapely.points(xy))
    saplings = marks[:, 0] >= 0.71
    forms = marks[:, 1] >= 0.5
    scales = 0.66 + 0.75 * np.mod(2 * marks[:, 1], 1)
    headings = 2 * np.pi * np.mod(17 * marks[:, 0], 1)
    for sapling, form in ((False, False), (False, True), (True, False), (True, True)):
        candidates = (saplings == sapling) & (forms == form) & np.isfinite(elevations)
        if not candidates.any():
            continue
        products, reach = prototype(sapling, form)
        group = candidates & (clearance > reach * scales)
        if group.any():
            species = "sapling" if sapling else "shrub"
            poses = np.column_stack((anchors[group], headings[group], scales[group]))
            yield species, int(form), products, poses
