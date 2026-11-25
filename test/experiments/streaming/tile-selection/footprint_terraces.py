"""Build camera-neutral roof terraces from seated source footprints."""

import numpy as np
from grid_building_masses import rectangles
from shapely import contains_xy

GROUP_METRES = 256
HEIGHT_STEP_M = 2
SAMPLES_PER_EDGE = 4
MIN_COVERED_SAMPLES = SAMPLES_PER_EDGE**2 // 2
NO_ROOF = np.iinfo(np.int16).min
NO_FLOOR = np.iinfo(np.int16).max


def raster(parts, spacing):
    bounds = np.array([part.footprint.bounds for part in parts])
    start = np.floor(bounds[:, :2].min(axis=0) / spacing).astype(int)
    shape = np.ceil(bounds[:, 2:].max(axis=0) / spacing).astype(int) - start
    origin = start * spacing
    fine_shape = shape * SAMPLES_PER_EDGE
    fine_spacing = spacing / SAMPLES_PER_EDGE
    roof = np.full(tuple(fine_shape[::-1]), NO_ROOF, dtype=np.int16)
    floor = np.full(roof.shape, NO_FLOOR, dtype=np.int16)
    for part, box in zip(parts, bounds):
        left, top = np.maximum(0, np.floor((box[:2] - origin) / fine_spacing)).astype(
            int
        )
        right, bottom = np.minimum(
            fine_shape, np.ceil((box[2:] - origin) / fine_spacing)
        ).astype(int)
        x = origin[0] + (np.arange(left, right) + 0.5) * fine_spacing
        z = origin[1] + (np.arange(top, bottom) + 0.5) * fine_spacing
        covered = contains_xy(part.footprint, x[None, :], z[:, None])
        upper = round(part.top_m / HEIGHT_STEP_M)
        lower = min(upper - 1, round(part.bottom_m / HEIGHT_STEP_M))
        if not NO_ROOF < lower < upper < NO_FLOOR:
            raise ValueError("footprint height exceeds terrace encoding")
        region = np.s_[top:bottom, left:right]
        np.maximum(roof[region], upper, where=covered, out=roof[region])
        np.minimum(floor[region], lower, where=covered, out=floor[region])
    cells = (shape[1], SAMPLES_PER_EDGE, shape[0], SAMPLES_PER_EDGE)
    roof_cells, floor_cells = roof.reshape(cells), floor.reshape(cells)
    occupied = np.count_nonzero(roof_cells != NO_ROOF, axis=(1, 3))
    upper = roof_cells.max(axis=(1, 3))
    lower = floor_cells.min(axis=(1, 3))
    vacant = occupied < MIN_COVERED_SAMPLES
    upper[vacant] = NO_ROOF
    lower[vacant] = NO_FLOOR
    return start, lower, upper


def wall_runs(lower, upper, neighbour_lower, neighbour_upper):
    exposed_spans = (
        (lower, np.minimum(upper, neighbour_lower)),
        (
            np.maximum(lower, neighbour_upper),
            np.where(neighbour_upper != NO_ROOF, upper, NO_ROOF),
        ),
    )
    for starts, stops in exposed_spans:
        for line, (low, high) in enumerate(zip(starts, stops)):
            edges = np.r_[
                0,
                np.flatnonzero((low[1:] != low[:-1]) | (high[1:] != high[:-1])) + 1,
                len(low),
            ]
            for at in range(len(edges) - 1):
                first, last = edges[at], edges[at + 1]
                if high[first] > low[first]:
                    yield line, first, last, low[first], high[first]


class Quads:
    def __init__(self):
        self.points = []
        self.normals = []
        self.indices = []

    def emit(self, corners, normal):
        first = len(self.points)
        self.points.extend(corners)
        self.normals.extend([normal] * 4)
        self.indices.extend(first + delta for delta in (0, 1, 2, 0, 2, 3))

    def mesh(self):
        return (
            np.asarray(self.points, dtype="<f4").ravel(),
            np.asarray(self.normals, dtype="<f4").ravel(),
            np.asarray(self.indices, dtype="<u4"),
        )


def surface(start, spacing, floor, roof, region):
    left, north, right, south = region
    domain = np.s_[north + 1 : south + 1, left + 1 : right + 1]
    lower, upper = floor[domain], roof[domain]
    walls, roofs = Quads(), Quads()
    levels = np.where(upper == NO_ROOF, 0, upper.astype(np.int32) - NO_ROOF)
    x_offset, z_offset = start + (left, north)
    for xmin, zmin, xmax, zmax, level in rectangles(levels):
        x0, x1 = (xmin + x_offset) * spacing, (xmax + x_offset) * spacing
        z0, z1 = (zmin + z_offset) * spacing, (zmax + z_offset) * spacing
        y = (level + NO_ROOF) * HEIGHT_STEP_M
        roofs.emit(((x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)), (0, 1, 0))
    for axis in (0, 2):
        if axis == 0:
            source = lower.T, upper.T
            fixed, offset = x_offset, z_offset
        else:
            source = lower, upper
            fixed, offset = z_offset, x_offset
        for sign in (-1, 1):
            dz, dx = (0, sign) if axis == 0 else (sign, 0)
            neighbour = np.s_[
                north + 1 + dz : south + 1 + dz, left + 1 + dx : right + 1 + dx
            ]
            other = (
                (floor[neighbour].T, roof[neighbour].T)
                if axis == 0
                else (floor[neighbour], roof[neighbour])
            )
            for line, first, last, bottom, top in wall_runs(*source, *other):
                y0, y1 = int(bottom) * HEIGHT_STEP_M, int(top) * HEIGHT_STEP_M
                u0, u1 = (offset + first) * spacing, (offset + last) * spacing
                at = (fixed + line + int(sign > 0)) * spacing
                if axis == 0:
                    quad = ((at, y0, u0), (at, y1, u0), (at, y1, u1), (at, y0, u1))
                    normal = (sign, 0, 0)
                else:
                    quad = ((u0, y0, at), (u1, y0, at), (u1, y1, at), (u0, y1, at))
                    normal = (0, 0, sign)
                walls.emit(quad if sign > 0 else quad[::-1], normal)
    return walls.mesh(), roofs.mesh()


def terraces(grounded, spacing):
    if not grounded:
        return
    if spacing <= 0 or GROUP_METRES % spacing:
        raise ValueError("terrace spacing must divide the group extent")
    start, lower, upper = raster(grounded, spacing)
    floor = np.pad(lower, 1, constant_values=NO_FLOOR)
    roof = np.pad(upper, 1, constant_values=NO_ROOF)
    height, width = upper.shape
    side = GROUP_METRES // spacing
    for z in range(0, height, side):
        for x in range(0, width, side):
            right, south = min(x + side, width), min(z + side, height)
            if (upper[z:south, x:right] != NO_ROOF).any():
                yield surface(start, spacing, floor, roof, (x, z, right, south))
