"""Whole polygon paving on held terrain; no terrain change or routing authority."""

from dataclasses import dataclass

import numpy as np
import shapely
from foot_ramp_geometry import close_caps
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._affine import fit_samples
from stair_module_topology import check_surface
from stair_seed_precision import GRID_M, model


@dataclass(frozen=True)
class AreaRecipe:
    maximum_grade: float = 0.1
    minimum_rise_m: float = 0.04
    nominal_rise_m: float = 0.05
    maximum_rise_m: float = 0.1
    nominal_embed_m: float = 0.02
    minimum_embed_m: float = 0.01
    terrain_faces_most: int = 512
    coordinates_most: int = 4096
    coordinate_face_pairs_most: int = 262144

    def __post_init__(self):
        values = (
            self.maximum_grade,
            self.minimum_rise_m,
            self.nominal_rise_m,
            self.maximum_rise_m,
            self.nominal_embed_m,
            self.minimum_embed_m,
        )
        if (
            any(type(v) not in (int, float) for v in values)
            or not np.isfinite(values).all()
            or not 0 < self.maximum_grade <= 1
            or not 0 < self.minimum_rise_m < self.nominal_rise_m < self.maximum_rise_m
            or not 0 < self.minimum_embed_m < self.nominal_embed_m
        ):
            raise ValueError("Finite ordered physical paving dimensions required")
        if (
            type(self.terrain_faces_most) is not int
            or not 1 <= self.terrain_faces_most <= 512
            or type(self.coordinates_most) is not int
            or not 3 <= self.coordinates_most <= 4096
            or type(self.coordinate_face_pairs_most) is not int
            or not 1 <= self.coordinate_face_pairs_most <= 262144
        ):
            raise ValueError(
                "Bounded local terrain and whole polygon capacities required"
            )


def footprint(original, recipe):
    if (
        not isinstance(original, shapely.Polygon)
        or original.is_empty
        or not original.is_valid
        or original.has_z
        or not 4 <= shapely.get_num_coordinates(original) <= recipe.coordinates_most
    ):
        raise ValueError("Bounded valid whole two-dimensional polygon required")
    if not np.isfinite(shapely.get_coordinates(original)).all():
        raise ValueError("Finite original polygon coordinates required")
    result = model(original, GRID_M)
    if shapely.get_num_coordinates(result) > recipe.coordinates_most:
        raise ValueError("Whole model footprint exceeds coordinate budget")
    return result


def planes(surface):
    xyz = surface.vertices[surface.triangles]
    matrix = xyz[:, 1:, :2] - xyz[:, :1, :2]
    if np.any(np.linalg.det(matrix) <= 0):
        raise ValueError("Upward nondegenerate terrain height-field triangles required")
    gradient = np.linalg.solve(matrix, (xyz[:, 1:, 2] - xyz[:, :1, 2])[..., None])[
        ..., 0
    ]
    return xyz[:, 0], gradient


def prepare(original, surface, recipe):
    polygon = footprint(original, recipe)
    if (
        not isinstance(surface, Mesh)
        or not 1 <= len(surface.triangles) <= recipe.terrain_faces_most
        or len(surface.vertices) > 3 * len(surface.triangles)
    ):
        raise ValueError("Bounded held local terrain required before paving work")
    if len(np.unique(surface.triangles)) != len(surface.vertices):
        raise ValueError("Local terrain must contain only its actually used vertices")
    if (
        len(surface.triangles) * shapely.get_num_coordinates(polygon)
        > recipe.coordinate_face_pairs_most
    ):
        raise ValueError("Terrain/polygon work budget exhausted before intersection")
    origin, gradient = planes(surface)
    triangles = shapely.polygons(surface.vertices[surface.triangles, :2])
    if not shapely.coverage_is_valid(triangles) or not shapely.coverage_union_all(
        triangles
    ).covers(polygon):
        raise ValueError(
            "Complete original area must be covered by one actual terrain field"
        )
    cells = shapely.intersection(triangles, polygon)
    counts = shapely.get_num_coordinates(cells)
    if not 3 <= counts.sum() <= recipe.coordinates_most:
        raise ValueError("Bounded complete terrain/polygon refinement required")
    xy = shapely.get_coordinates(cells)
    owner = np.repeat(np.arange(len(cells)), counts)
    heights = origin[owner, 2] + np.einsum(
        "ij,ij->i", xy - origin[owner, :2], gradient[owner]
    )
    return {
        "polygon": polygon,
        "surface": surface,
        "terrain_origins": origin,
        "terrain_gradients": gradient,
        "control_xy": xy,
        "control_z": heights,
    }


def choose_plane(prepared, recipe, method):
    if method == "facet":
        candidates = [
            (p[:2], p[2], g)
            for p, g in zip(
                prepared["terrain_origins"], prepared["terrain_gradients"], strict=True
            )
            if np.linalg.norm(g) <= recipe.maximum_grade
        ]
    elif method == "affine":
        samples = np.c_[prepared["control_xy"], prepared["control_z"]]
        origin, height, gradient, _error, _grade = fit_samples(
            samples, np.array([np.eye(2) / recipe.maximum_grade])
        )
        candidates = [(origin, height, gradient)]
    elif method == "highs":
        candidates = [highs_plane(prepared, recipe)]
    else:
        raise ValueError(
            "Existing facet, affine samples or bounded HiGHS control required"
        )
    if not candidates:
        raise ValueError("No physical candidate satisfies the requested grade")
    found = []
    for origin, height, gradient in candidates:
        residual = (
            height
            + (prepared["control_xy"] - origin) @ gradient
            - prepared["control_z"]
        )
        low, high = float(residual.min()), float(residual.max())
        found.append((high - low, origin, height, gradient, low, high))
    spread, origin, height, gradient, low, high = min(
        found, key=lambda c: (c[0], float(c[2]), tuple(c[3]), tuple(c[1]))
    )
    if recipe.nominal_rise_m + spread > recipe.maximum_rise_m:
        raise ValueError("Whole terrain relief exceeds the declared paving recipe")
    return {
        "origin": origin,
        "height": height,
        "gradient": gradient,
        "upper_shift": recipe.nominal_rise_m - low,
        "lower_shift": -recipe.nominal_embed_m - high,
        "terrain_relief_m": spread,
        "method": method,
        "plane_candidates": len(candidates),
    }


def highs_plane(prepared, recipe):
    from scipy.optimize import linprog

    xy, z = prepared["control_xy"], prepared["control_z"]
    origin, datum = xy.mean(axis=0), float(z.mean())
    design = np.c_[np.ones(len(xy)), xy - origin]
    grade = np.array([[0, sx, sy, 0] for sx in (-1, 1) for sy in (-1, 1)])
    matrix = np.vstack(
        (np.c_[design, -np.ones(len(xy))], np.c_[-design, -np.ones(len(xy))], grade)
    )
    bounds = np.r_[z - datum, datum - z, np.full(4, recipe.maximum_grade)]
    result = linprog(
        [0, 0, 0, 1],
        A_ub=matrix,
        b_ub=bounds,
        bounds=[(None, None)] * 3 + [(0, None)],
        method="highs",
        options={"time_limit": 1.0},
    )
    if not result.success or result.status != 0 or not np.isfinite(result.x).all():
        raise ValueError("Local paving plane fit did not finish successfully")
    # The L1 grade cap is conservative for the physical Euclidean grade limit.
    return origin, datum + float(result.x[0]), result.x[1:3]


def caps(polygon, plane, recipe):
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    if not 1 <= len(triangles) <= 2 * recipe.coordinates_most:
        raise ValueError("Bounded complete paving triangulation required")
    soup = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(soup.reshape(-1, 2), axis=0, return_inverse=True)
    faces = inverse.reshape(-1, 3).astype(np.uint32)
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    reverse = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] < 0
    faces[reverse] = faces[reverse][:, [0, 2, 1]]
    check_surface(xy, faces)
    if not shapely.coverage_union_all(shapely.polygons(xy[faces])).equals(polygon):
        raise ValueError("Complete paving boundary and holes must be retained")
    z = plane["height"] + (xy - plane["origin"]) @ plane["gradient"]
    return (
        Mesh(np.c_[xy, z + plane["upper_shift"]], faces),
        Mesh(np.c_[xy, z + plane["lower_shift"]], faces),
    )


def construct(prepared, recipe, method):
    plane = choose_plane(prepared, recipe, method)
    upper, lower = caps(prepared["polygon"], plane, recipe)
    body = close_caps(upper, lower)
    return body, upper, lower, plane
