"""Transport products consumed by buildings and vegetation; solver migration is incremental."""

from dataclasses import dataclass

from ..contracts import (
    FootprintAdjustment,
    Hint,
    LocalFrame,
    Mesh,
    Reservation,
    collection,
)
from .surface import close_surface
from .heights import HeightBudget, HeightFit, fit_surface_heights
from .vertical import (
    VerticalTransition,
    minimum_transition_length,
    plan_vertical_transition,
    sample_vertical_transition,
)
from .profile_surface import mesh_projected_profile
from .endpoint_grades import EndpointTransition, plan_endpoint_transition
from .port_heights import (
    PortHeightBudget, PortHeightInput, PortHeightPlan, PortHeightFit,
    PortPlaneField, plan_port_heights,
)


@dataclass(frozen=True, slots=True)
class Plan:
    frame: LocalFrame
    adjustments: tuple[FootprintAdjustment, ...] = ()
    reservations: tuple[Reservation, ...] = ()
    hints: tuple[Hint, ...] = ()
    meshes: tuple[Mesh, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.frame, LocalFrame):
            raise ValueError("one explicit local frame is required")
        collection(self.adjustments, FootprintAdjustment, "footprint adjustments")
        collection(self.reservations, Reservation, "transport reservations")
        collection(self.hints, Hint, "transport hints")
        collection(self.meshes, Mesh, "transport meshes")
