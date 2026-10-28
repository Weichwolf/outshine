"""Independent unfitted perimeter probes; sampled errors are not global bounds."""

import numpy as np
import shapely


def ordered_rim(result):
    if "rim_profile" in result:
        return result["mesh"].vertices[result["rim_profile"]["ordered_vertex_ids"]]
    xy = np.asarray(result["domain"].exterior.coords)[:-1]
    vertices = result["mesh"].vertices[result["outer_vertex_ids"]]
    lookup = {tuple(p[:2]): p for p in vertices}
    return np.array([lookup[tuple(p)] for p in xy])


def validation_points(result, count=512):
    xy = np.asarray(shapely.box(*result["domain"].bounds).exterior.coords)[:-1]
    if result.get("rim_profile", {}).get("recipe", {}).get("boundary") == "oriented":
        xy = result["rim_profile"]["rectangle_corners_xy"]
    arc = np.r_[0.0, np.cumsum(np.linalg.norm(np.roll(xy, -1, axis=0) - xy, axis=1))]
    at = (np.arange(count) + 0.37) / count * arc[-1]
    queries = np.c_[
        np.interp(at, arc, np.r_[xy[:, 0], xy[0, 0]]),
        np.interp(at, arc, np.r_[xy[:, 1], xy[0, 1]]),
    ]
    return at, queries


def validate_rim(result, at, observed):
    vertices = ordered_rim(result)
    arc = np.r_[
        0.0,
        np.cumsum(
            np.linalg.norm(
                np.roll(vertices[:, :2], -1, axis=0) - vertices[:, :2], axis=1
            )
        ),
    ]
    predicted = np.interp(at, arc, np.r_[vertices[:, 2], vertices[0, 2]])
    delta = predicted - observed
    return {
        "count": len(at),
        "maximum_abs_error_m": float(abs(delta).max()),
        "rms_error_m": float(np.sqrt(np.mean(delta * delta))),
        "scope": "Same unfitted geometric perimeter queries against the bound DEM; sampled error only, no continuous/global/original-grid error certificate.",
    }, predicted


def compare_domains(context, results, *, spacing=0.5, maximum_grid_points=65536):
    """Use identical unfitted XY controls where the two actual soil domains overlap."""
    from matplotlib.tri import LinearTriInterpolator, Triangulation

    if not np.isfinite(spacing) or spacing <= 0:
        raise ValueError("Positive finite validation spacing required")
    domains = {name: result["domain"] for name, result in results.items()}
    x0, y0, x1, y1 = shapely.union_all(list(domains.values())).bounds
    low = np.floor(np.array([x0, y0]) / spacing)
    high = np.ceil(np.array([x1, y1]) / spacing)
    count = (high - low + 1).astype(np.int64)
    if any(count <= 0) or int(count[0]) * int(count[1]) > maximum_grid_points:
        raise ValueError("Common terrain validation grid budget")
    x, y = [
        (np.arange(n) + start) * spacing for n, start in zip(count, low, strict=True)
    ]
    xy = np.stack(np.meshgrid(x, y), axis=-1).reshape(-1, 2)
    points = shapely.points(xy)
    road = context.road
    footprint = shapely.union_all(shapely.polygons(road.vertices[road.triangles, :2]))
    inside = {
        name: shapely.contains(domain, points) for name, domain in domains.items()
    }
    soil = ~shapely.covers(footprint, points)
    common = np.logical_and.reduce(list(inside.values())) & soil
    selected = np.logical_or.reduce(list(inside.values()))
    xy = xy[selected]
    observed = context.sample_z(xy)
    common, soil = common[selected], soil[selected]
    if not common.any():
        raise ValueError("No common terrain validation controls")
    arrays = {"validation_xy": xy, "validation_dem_z": observed, "common_soil": common}
    report = {"spacing_m": spacing, "acquired_dem_points": len(xy), "methods": {}}
    for name, result in results.items():
        vertices, faces = result["mesh"].vertices, result["mesh"].triangles
        field = LinearTriInterpolator(
            Triangulation(vertices[:, 0], vertices[:, 1], faces), vertices[:, 2]
        )
        predicted = np.asarray(field(xy[:, 0], xy[:, 1]).filled(np.nan))
        error = predicted - observed
        own = inside[name][selected] & soil
        metrics = {}
        for label, mask in (("same_common_soil", common), ("own_soil_domain", own)):
            if not mask.any() or not np.isfinite(error[mask]).all():
                raise ValueError("Uncovered actual terrain validation controls")
            metrics[label] = {
                "points": int(mask.sum()),
                "rms_m": float(np.sqrt(np.mean(error[mask] ** 2))),
                "maximum_abs_m": float(abs(error[mask]).max()),
            }
        report["methods"][name] = metrics
        arrays[name + "_error"] = error
        arrays[name + "_soil"] = own
    report["scope"] = (
        "Same fixed XY grid on actual common soil; separate own-domain controls. "
        "Sampled errors, no continuous bound or surrounding terrain seam proof."
    )
    return report, arrays
