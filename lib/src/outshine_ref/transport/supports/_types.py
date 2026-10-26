"""Owned support products and bounded geometric work; physical recipes are caller-owned."""

from dataclasses import dataclass
from math import isfinite

from ...contracts import Mesh


@dataclass(frozen=True, slots=True)
class SupportBudget:
    maximum_surface_faces: int = 262144
    maximum_patch_faces: int = 2048
    maximum_footprint_coordinates: int = 64
    maximum_segments: int = 8192
    maximum_contact_faces: int = 4096
    maximum_interface_candidates: int = 65536
    maximum_sample_candidates: int = 1000000
    seconds: float = 2.0

    def __post_init__(self):
        for name, ceiling in (
            ("maximum_surface_faces", 262144),
            ("maximum_patch_faces", 2048),
            ("maximum_footprint_coordinates", 4096),
            ("maximum_segments", 8192),
            ("maximum_contact_faces", 4096),
            ("maximum_interface_candidates", 1048576),
            ("maximum_sample_candidates", 1048576),
        ):
            value = getattr(self, name)
            if type(value) is not int or not 1 <= value <= ceiling:
                raise ValueError(
                    "bounded positive integer support budget required: " + name
                )
        if not isfinite(self.seconds) or not 0 < self.seconds <= 60:
            raise ValueError(
                "positive cooperative support time within sixty seconds required"
            )


@dataclass(frozen=True, slots=True, eq=False)
class SupportVolume:
    """Owned closed body and shared-XY whole lower/upper contact meshes.

    A builder verifies geometry; this record does not select surfaces, sources,
    load recipes, world frames or spatial reservations.
    """

    body: Mesh
    lower_contact: Mesh
    upper_contact: Mesh

    def __post_init__(self):
        if not all(
            isinstance(m, Mesh)
            for m in (self.body, self.lower_contact, self.upper_contact)
        ):
            raise ValueError("three owned support meshes required")
