"""Physical water height references; source admission and native products are caller-owned."""

__all__ = ["fit_physical_surface"]


def fit_physical_surface(
    mesh,
    anchor,
    rows,
    *,
    up_bounds_m,
    fixed=None,
    contact_points=None,
    seconds=5.0,
    iterations=200,
    normal_model="whole-prism",
    primary_rows=None,
    solver="clarabel",
):
    """Fit shared physical facets under explicit whole-prism direction intervals.

    Optional solver/geodesy imports occur on use. The complete datum, direction,
    primary-row and bounded-absence contract is documented in water_profiles.md.
    contact_points are explicit hard ENU XYZ positions on the selected surface.
    """
    from ._fit import fit_physical_surface as fit

    return fit(
        mesh,
        anchor,
        rows,
        up_bounds_m=up_bounds_m,
        fixed=fixed,
        contact_points=contact_points,
        seconds=seconds,
        iterations=iterations,
        normal_model=normal_model,
        primary_rows=primary_rows,
        solver=solver,
    )
