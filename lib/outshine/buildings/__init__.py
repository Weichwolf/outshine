"""Buildings consume adjusted footprints and publish occupancy and access products."""

from dataclasses import dataclass

from ..contracts import BuildingFootprint, Hint, LocalFrame, Mesh, Reservation, SourceInputs, collection


@dataclass(frozen=True, slots=True)
class Inputs:
    sources: SourceInputs
    footprints: tuple[BuildingFootprint, ...]
    reservations: tuple[Reservation, ...]
    hints: tuple[Hint, ...]


@dataclass(frozen=True, slots=True)
class Plan:
    frame: LocalFrame
    reservations: tuple[Reservation, ...] = ()
    hints: tuple[Hint, ...] = ()
    meshes: tuple[Mesh, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.frame, LocalFrame):
            raise ValueError("one explicit local frame is required")
        collection(self.reservations, Reservation, "building reservations")
        collection(self.hints, Hint, "building hints")
        collection(self.meshes, Mesh, "building meshes")

