"""Place regional herbs on the solved ground using mapped and satellite cover."""

from functools import cache

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROUTE_MATERIALS, surface_elevations
from outshine.terrain.land_cover import COVER_PRIORITY, cover_material
from outshine.vegetation.graminoid import graminoid_tuft_mesh
from outshine.vegetation.plant_sites import patch_density, scatter_mesh, tile_sites

OPEN_COVER = {"grass", "meadow", "scrub"}
OPEN_SURFACES = ("ground", "grass", "meadow")
REACH_M = 95.0
SAVANNA = 1
TUSSOCK = 2
ACACIA_COMMIPHORA_BUSHLANDS = 57
NORTH_ISLAND_TEMPERATE_FORESTS = 171


class RegionalOpenCover:
    def __init__(self, atlas, source, time):
        self.atlas = atlas
        self.source = source
        self.dry_savanna = time.month in (2, 6, 7, 8, 9, 10)
        covers = {}
        for layer, geometry, properties in source.features:
            name = None
            if layer in ("water", "building"):
                name = "water" if layer == "water" else "settlement"
            elif layer in ("landcover", "landuse"):
                name = cover_material(layer, properties)
            if name is not None:
                covers.setdefault(name, []).append(shapely.make_valid(geometry))
        self.overrides = tuple(
            (name in OPEN_COVER, shapely.union_all(covers[name]))
            for name in sorted(
                covers,
                key=lambda name: (
                    name in ("water", "settlement"),
                    COVER_PRIORITY.get(name, 0),
                ),
            )
        )
        for _, geometry in self.overrides:
            shapely.prepare(geometry)

    def classify(self, xy):
        lon, lat = (
            np.array([self.source.lon, self.source.lat]) + xy / self.source.metres
        ).T
        cover, ecoregion = self.atlas.land_cells(lat, lon)
        open_land = np.isin(cover, (20, 30))
        for allowed, geometry in self.overrides:
            open_land[shapely.intersects_xy(geometry, xy[:, 0], xy[:, 1])] = allowed
        groups = np.zeros(len(xy), dtype=np.uint8)
        groups[open_land & (ecoregion == ACACIA_COMMIPHORA_BUSHLANDS)] = SAVANNA
        upslope = open_land & (ecoregion == NORTH_ISLAND_TEMPERATE_FORESTS)
        if upslope.any():
            elevation = self.source.height(xy[upslope]) + self.source.datum
            groups[upslope] = np.where(elevation >= 1000, TUSSOCK, 0)
        return groups

    def replaces(self, xy):
        return self.classify(xy) != 0

    def batches(self, surfaces, eye, seed):
        candidates = list(
            tile_sites(
                self.source, eye[:2], REACH_M, seed ^ 0x23A9, density=6.2, cell_size=3
            )
        )
        if not candidates:
            return
        xy, marks = (np.concatenate(rows) for rows in zip(*candidates, strict=True))
        groups = self.classify(xy)
        patch = patch_density(self.source, xy, seed, scale=6)
        distance = np.linalg.norm(xy - eye[:2], axis=1)
        middle = np.clip((distance - 13) / 39, 0, 1)
        outer = np.clip((REACH_M - distance) / 23, 0, 1)
        distance_density = 1.0 - 0.84 * middle**2 * (3 - 2 * middle)
        savanna_cover = 0.29 + 0.70 * patch**1.4
        tussock_cover = 0.07 + 0.53 * patch**1.9
        probability = np.where(groups == SAVANNA, savanna_cover, tussock_cover)
        probability *= distance_density * outer
        selected = (groups != 0) & (marks[:, 2] < probability)
        if not selected.any():
            return
        xy, marks, groups, patch, distance = (
            values[selected] for values in (xy, marks, groups, patch, distance)
        )
        mesh = planting_surface(surfaces, eye[:2])
        if mesh is None:
            return
        heights = surface_elevations(mesh, xy)
        footprint = mesh_footprint(mesh)
        routes = planting_surface(surfaces, eye[:2], ROUTE_MATERIALS)
        if routes is not None:
            footprint = footprint.difference(mesh_footprint(routes))
        supported = np.isfinite(heights) & shapely.contains_xy(
            footprint, xy[:, 0], xy[:, 1]
        )
        if not supported.any():
            return
        xy, marks, groups, patch, distance, heights = (
            values[supported]
            for values in (xy, marks, groups, patch, distance, heights)
        )
        anchors = np.column_stack((xy, heights))
        clearance = shapely.distance(footprint.boundary, shapely.points(xy))
        scale = 0.62 + 0.58 * np.mod(marks[:, 0] * 19, 1) + 0.22 * patch
        yield from draw_tufts(
            anchors, marks, groups, distance, scale, clearance, self.dry_savanna
        )


def planting_surface(surfaces, centre, materials=OPEN_SURFACES):
    parts = []
    for material in materials:
        if material in surfaces:
            vertices, faces = surfaces[material]
            if not len(faces):
                continue
            triangles = np.asarray(vertices)[np.asarray(faces)]
            keep = np.all(
                triangles[..., :2].max(axis=1) >= centre - REACH_M - 1, axis=1
            )
            keep &= np.all(
                triangles[..., :2].min(axis=1) <= centre + REACH_M + 1, axis=1
            )
            parts.append(triangles[keep].reshape(-1, 3))
    if not parts or not any(len(part) for part in parts):
        return None
    vertices = np.concatenate(parts)
    return vertices, np.arange(len(vertices), dtype=np.int32).reshape(-1, 3)


def mesh_footprint(mesh):
    vertices, faces = mesh
    polygons = shapely.polygons(vertices[faces][..., :2])
    return shapely.union_all(polygons[shapely.area(polygons) > 1e-10])


@cache
def tuft_geometry(profile, detailed, variant):
    blades, shape = (
        (14, (1.30, 1.30, 0.69)) if profile == SAVANNA else (26, (1.95, 1.95, 1.55))
    )
    if not detailed:
        blades = (blades + 2) // 3
    seed = 0x9A43 + 113 * profile + 47 * variant
    mesh = graminoid_tuft_mesh(seed, blades, 5 if detailed else 3)
    vertices = mesh["vertices"] * shape
    rng = np.random.default_rng(seed ^ 128)
    splay = rng.uniform(0.1, 1.0, blades)[mesh["blade_index"]]
    radial_angle = mesh["blade_index"] * 2.39996 + rng.uniform(0, 2 * np.pi)
    directions = np.column_stack((np.cos(radial_angle), np.sin(radial_angle)))
    along = mesh["uv"][:, 1]
    spread, droop = (0.20, 0.12) if profile == SAVANNA else (0.43, 0.31)
    offset = splay * (0.02 + spread * along)
    vertices[:, :2] += directions * offset[:, None]
    vertices[:, 2] -= droop * splay * along**2
    vertices[:, 2] = np.maximum(vertices[:, 2], -0.025)
    radius = np.linalg.norm(vertices[:, :2], axis=1).max()
    return (vertices, mesh["faces"]), radius


def draw_tufts(anchors, marks, groups, distance, scale, clearance, dry_savanna):
    dry = marks[:, 1] < np.where(groups == TUSSOCK, 0.77, 0.82 if dry_savanna else 0.15)
    angles = 2 * np.pi * marks[:, 0]
    variants = np.floor(np.mod(marks[:, 0] * 11, 1) * 3).astype(int)
    near = distance < 21 + 7 * marks[:, 1]
    for profile, green, straw in (
        (SAVANNA, "savanna grass blades", "savanna straw blades"),
        (TUSSOCK, "tussock green blades", "tussock straw blades"),
    ):
        for detailed in (False, True):
            for variant in range(3):
                group = (groups == profile) & (near == detailed) & (variants == variant)
                if not group.any():
                    continue
                geometry, radius = tuft_geometry(profile, detailed, variant)
                fitted = np.minimum(scale, (clearance - 0.015) / radius)
                group &= fitted >= 0.35
                for name, mask in ((green, group & ~dry), (straw, group & dry)):
                    if mask.any():
                        yield (
                            name,
                            *scatter_mesh(
                                geometry, anchors[mask], angles[mask], fitted[mask]
                            ),
                        )
