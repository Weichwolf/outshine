"""Shared full-width C1 port fields and their bounded C0 road/soil meshes."""

from ._budget import DEFAULT_BUDGET, PortHeightBudget
from ._field import PlaneField as PortPlaneField
from ._input import PortHeightInput
from ._plan import PortHeightFit, PortHeightPlan

__all__ = [
    "PortHeightBudget",
    "PortHeightFit",
    "PortHeightInput",
    "PortHeightPlan",
    "PortPlaneField",
    "plan_port_heights",
]


def plan_port_heights(source, *, budget=DEFAULT_BUDGET, method="cone", progress=None):
    """Fit whole C1 road ports and a checked C0 road/soil mesh within offline limits.

    A failed local solve returns no plan and an explicit reason per original part.
    Inputs retain caller-bound frame/kind/source metadata; finite contacts use the
    actual output mesh. Solver imports and sparse assembly are deferred until use.
    See PortHeightInput, PortHeightBudget and PortHeightPlan for their contracts.
    """
    from ._api import plan_port_heights as run

    return run(source, budget=budget, method=method, progress=progress)
