"""Closed additions checked against actual source openings, walls and route tubes."""

import numpy as np
import shapely
from bay_facade_access import check_routes
from building_plan import FLOOR_M
from clad_facade_mesh import surface
from facade_frame_geometry import placed
from structural_shell_geometry import native


def apertures(terrace):
    mass = terrace.prepared.mass
    core = mass.core
    values = [
        (
            np.asarray(core.origin_xy)
            + w.centre[0] * np.asarray(core.across_xy)
            + w.centre[1] * np.asarray(core.along_xy),
            w.bottom_m,
            w.width_m,
            w.height_m,
        )
        for w in (*terrace.prepared.facade.windows, *terrace.portals)
    ]
    values.append(
        (
            np.asarray(mass.entry.centre),
            mass.bottom_m + FLOOR_M,
            1.10,
            mass.entry.height_m,
        )
    )
    return values


def opening_checks(solid, layer, openings):
    tangent = np.asarray(layer.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    rays = 0
    for point, low, width, height in openings:
        relative = point - layer.origin_xy
        if abs(float(relative @ normal) - 0.005) > 1e-7:
            continue
        u, v = float(relative @ tangent), low - layer.base_m
        opening = shapely.box(u - width / 2, v, u + width / 2, v + height)
        if not opening.intersects(shapely.box(*layer.region.bounds)):
            continue
        if layer.region.intersection(opening).area >= 1e-9:
            raise ValueError("Actual source opening intersects an added facade field")
        for dx in (-0.35, 0, 0.35):
            for dz in (0.15, 0.5, 0.85):
                p = [u + dx * width, v + dz * height]
                if solid.ray_cast([*p, -0.01], [*p, layer.depth_m + 0.01]):
                    raise ValueError("Added closed mesh blocks a source aperture ray")
                rays += 1
    return rays


def attachment(raw, layer, storeys):
    mesh = placed(raw, layer, layer.material)
    back = np.all(raw["vertices"][raw["faces"], 2] == 0, axis=1)
    samples = mesh["vertices"][mesh["faces"]][back].mean(axis=1)
    for point in samples:
        xy = shapely.Point(point[:2])
        if not any(
            s.bottom_m - 1e-9 <= point[2] <= s.top_m + 1e-9
            and polygon.buffer(1e-8).covers(xy)
            and polygon.boundary.distance(xy) < 0.01
            for s, polygon in storeys
        ):
            raise ValueError("Added facade back does not attach to its source wall")
    return len(samples)


def inspect(planned, terrace):
    openings = apertures(terrace)
    storeys = [
        (s, shapely.from_wkb(s.footprint_wkb)) for s in terrace.prepared.mass.storeys
    ]
    layers, rays, backs, maximum = 0, 0, 0, 0.0
    for cell in planned.cells:
        for layer in cell.layers:
            raw = surface(layer, "closed")
            solid = native(raw)
            expected = layer.region.area * layer.depth_m
            error = abs(solid.volume() - expected)
            if expected <= 0 or not np.isfinite(error) or error > 1e-7:
                raise ValueError("Closed facade layer has incorrect positive volume")
            if (
                layer.region.intersection(shapely.from_wkb(layer.protected_wkb)).area
                > 1e-10
            ):
                raise ValueError("Closed facade layer intersects protected reserves")
            rays += opening_checks(solid, layer, openings)
            backs += attachment(raw, layer, storeys)
            maximum = max(maximum, error)
            layers += 1
    return {
        "closed_layers": layers,
        "actual_opening_rays": rays,
        "source_wall_back_samples": backs,
        "maximum_volume_error_m3": maximum,
        "routes": check_routes(planned, terrace.prepared, terrace.portals),
        "scope": "Closed additions, actual aperture rays and source-wall samples; continuous existing access/terrace route tubes. Original body/roof audit required separately; no arbitrary roaming, runtime LOD, shadow or device admission.",
    }
