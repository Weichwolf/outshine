"""Experiment compatibility entry; all station-slice construction lives in lib."""

from outshine_ref.geometry import ProjectedSurfaceBudget, projected_height_surface


def projected_mesh(domain, origin, direction, table, *, seconds=2.0, max_faces=8192):
    mesh, report = projected_height_surface(
        domain,
        origin,
        direction,
        table,
        budget=ProjectedSurfaceBudget(time_limit_s=seconds, max_faces=max_faces),
    )
    if mesh is None:
        raise ValueError(report["reason"] + ": " + report.get("detail", ""))
    return mesh, report
