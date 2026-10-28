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


def domain_grid(domains, spacing, maximum):
    """Filter bounded scanline spans before retaining the shared fixed XY grid."""
    x0, y0, x1, y1 = shapely.union_all(list(domains.values())).bounds
    rows = np.array([np.floor(y0 / spacing), np.ceil(y1 / spacing)])
    if not np.isfinite(rows).all() or rows[1] - rows[0] + 1 > maximum:
        raise ValueError("Common terrain validation grid budget")
    chunks, retained, tested = [], 0, 0
    for row in range(int(rows[0]), int(rows[1]) + 1):
        y = row * spacing
        scanline = shapely.LineString([[x0, y], [x1, y]])
        spans = []
        for domain in domains.values():
            cut = shapely.intersection(domain, scanline)
            if not cut.is_empty:
                a, _, b, _ = cut.bounds
                spans.append((int(np.floor(a / spacing)), int(np.ceil(b / spacing))))
        merged = []
        for a, b in sorted(spans):
            if merged and a <= merged[-1][1] + 1:
                merged[-1] = (merged[-1][0], max(b, merged[-1][1]))
            else:
                merged.append((a, b))
        for a, b in merged:
            if b - a + 1 > maximum:
                raise ValueError("Common terrain validation scanline budget")
            x = np.arange(a, b + 1) * spacing
            xy = np.c_[x, np.full(len(x), y)]
            points = shapely.points(xy)
            keep = np.logical_or.reduce(
                [shapely.contains(domain, points) for domain in domains.values()]
            )
            tested += len(xy)
            retained += int(keep.sum())
            if retained > maximum:
                raise ValueError("Common terrain validation grid budget")
            if keep.any():
                chunks.append(xy[keep])
    if not chunks:
        raise ValueError("No terrain validation controls")
    return np.vstack(chunks), tested


def compare_domains(context, results, *, spacing=0.5, maximum_grid_points=65536):
    """Use identical unfitted XY controls where the two actual soil domains overlap."""
    from matplotlib.tri import LinearTriInterpolator, Triangulation

    if not np.isfinite(spacing) or spacing <= 0:
        raise ValueError("Positive finite validation spacing required")
    domains = {name: result["domain"] for name, result in results.items()}
    xy, tested = domain_grid(domains, spacing, maximum_grid_points)
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
    report = {
        "spacing_m": spacing,
        "generated_candidate_grid_points": tested,
        "acquired_dem_points": len(xy),
        "methods": {},
    }
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
