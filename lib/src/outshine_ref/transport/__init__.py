"""Transport products consumed by buildings and vegetation; solver migration is incremental."""

from dataclasses import dataclass

from ..contracts import FootprintAdjustment, Hint, LocalFrame, Mesh, Reservation, collection
from .surface import close_surface


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

