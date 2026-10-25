"""Source-projected area fractions on fixed small parent-local grids."""

import numpy as np
import shapely

PLANES = ((0, 1), (0, 2), (1, 2))
SIZE = 8


def source_projections(sources):
    result = [shapely.union_all([s.footprint for s in sources])]
    for axis in (0, 1):
        result.append(
            shapely.union_all(
                [
                    shapely.box(
                        p.bounds[axis], s.bottom[0], p.bounds[axis + 2], s.roof[1]
                    )
                    for s in sources
                    for p in shapely.get_parts(s.footprint)
                ]
            )
        )
    return result


def proxy_projections(positions, indices):
    triangles = np.asarray(positions, float)[indices]
    return [
        shapely.union_all(
            [p for p in shapely.polygons(triangles[:, :, axes]) if p.area > 0]
        )
        for axes in PLANES
    ]


def area_field(source, proxy):
    bounds = np.asarray(proxy.bounds).reshape(2, 2)
    if proxy.is_empty or not proxy.is_valid or np.any(bounds[1] <= bounds[0]):
        raise ValueError("finite nonempty projected parent area required")
    edges = np.linspace(bounds[0], bounds[1], SIZE + 1)
    x, y = np.meshgrid(np.arange(SIZE), np.arange(SIZE), indexing="xy")
    cells = shapely.box(edges[x, 0], edges[y, 1], edges[x + 1, 0], edges[y + 1, 1])
    parent = shapely.intersection(cells, proxy)
    denominator = shapely.area(parent)
    covered = shapely.area(shapely.intersection(parent, source))
    values = np.divide(
        covered, denominator, out=np.zeros_like(covered), where=denominator > 0
    )
    if (
        not np.isfinite(values).all()
        or np.any(values < 0)
        or np.any(values > 1 + 1e-10)
    ):
        raise ValueError("bounded source area fractions required")
    return (
        np.minimum(values, 1).astype(np.float32),
        bounds,
        {
            "source_area_m2": float(source.area),
            "proxy_area_m2": float(proxy.area),
            "source_outside_proxy_m2": float(shapely.difference(source, proxy).area),
            "covered_area_m2": float(covered.sum()),
            "reconstructed_area_m2": float(
                np.sum(values.astype(np.float32) * denominator)
            ),
            "cell_intersections": SIZE * SIZE * 2,
        },
    )


def build_field(sources, positions, indices):
    fields = [
        area_field(source, proxy)
        for source, proxy in zip(
            source_projections(sources),
            proxy_projections(positions, indices),
            strict=True,
        )
    ]
    return (
        {
            "values": np.stack([f[0] for f in fields]),
            "bounds": np.stack([f[1] for f in fields]),
        },
        [f[2] for f in fields],
    )


def validate_field(field, groups):
    values, bounds = field["values"], field["bounds"]
    if (
        values.shape != (groups, 3, SIZE, SIZE)
        or bounds.shape != (groups, 3, 2, 2)
        or values.dtype != np.float32
        or not np.isfinite(values).all()
        or not np.isfinite(bounds).all()
        or np.any((values < 0) | (values > 1))
        or np.any(bounds[:, :, 1] <= bounds[:, :, 0])
    ):
        raise ValueError("qualified fixed-size finite coverage fields required")


def sample_field(points, normals, field, group):
    values, bounds = field["values"][group], field["bounds"][group]
    response = np.empty((len(points), 3))
    for plane, axes in enumerate(PLANES):
        uv = (points[:, axes] - bounds[plane, 0]) / (
            bounds[plane, 1] - bounds[plane, 0]
        )
        at = np.floor(np.clip(uv, 0, 1) * SIZE).astype(int).clip(0, SIZE - 1)
        response[:, plane] = values[plane, at[:, 1], at[:, 0]]
    weights = np.abs(normals[:, [2, 1, 0]])
    return np.sum(response * weights, axis=1) / weights.sum(axis=1)
