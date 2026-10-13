"""Vegetation consumes binding spaces and optional hints from both predecessors."""

from dataclasses import dataclass

from ..contracts import Hint, LocalFrame, Mesh, Reservation, SourceInputs, collection


@dataclass(frozen=True, slots=True)
class Inputs:
    sources: SourceInputs
    reservations: tuple[Reservation, ...]
    hints: tuple[Hint, ...]


@dataclass(frozen=True, slots=True)
class Plan:
    frame: LocalFrame
    meshes: tuple[Mesh, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.frame, LocalFrame):
            raise ValueError("one explicit local frame is required")
        collection(self.meshes, Mesh, "vegetation meshes")

