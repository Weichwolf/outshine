"""Own flat frond source surfaces; organ plans precede requested mesh detail."""

import hashlib
import json
from dataclasses import asdict, dataclass
from functools import cached_property

import numpy as np

from .blade_surface import Blade
from .frond_surface import MAX_TRIANGLES, MAX_VERTICES


@dataclass(frozen=True)
class PinnatePattern:
    seed: int = 17
    length: float = 1.3
    pairs: int = 16
    leaflet_length: float = 0.30
    aspect: float = 0.20
    angle: float = 1.22
    first: float = 0.12
    last: float = 0.92
    variance: float = 0.08

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or type(self.pairs) is not int
            or not 1 <= self.pairs <= 64
        ):
            raise ValueError("stable seed and one to 64 leaflet pairs required")
        values = (
            self.length,
            self.leaflet_length,
            self.aspect,
            self.angle,
            self.first,
            self.last,
            self.variance,
        )
        if (
            not all(np.isfinite(v) for v in values)
            or min(self.length, self.leaflet_length, self.aspect) <= 0
        ):
            raise ValueError("finite positive physical pattern dimensions required")
        if (
            not 0 < self.first <= self.last < 1
            or not 0 <= self.variance < 0.5
            or not 0 < self.angle < np.pi / 2
        ):
            raise ValueError(
                "ordered parent interval and outward leaflet directions required"
            )

    def recipe(self):
        return {"version": "pinnate-source-1", **asdict(self)}

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def organs(self):
        result = []
        for pair, parameter in enumerate(
            np.linspace(self.first, self.last, self.pairs)
        ):
            for side, sign in enumerate((-1, 1)):
                identity = 2 * pair + side
                rng = np.random.default_rng(
                    np.random.SeedSequence((self.seed, identity))
                )
                length = (
                    self.leaflet_length
                    * (0.10 + 0.90 * np.sin(np.pi * parameter))
                    * (1 + rng.uniform(-self.variance, self.variance))
                )
                length = min(length, (1 - parameter) * self.length / np.cos(self.angle))
                cosine, sine = np.cos(self.angle), sign * np.sin(self.angle)
                result.append(
                    {
                        "id": identity,
                        "origin": (0, self.length * parameter, 0),
                        "frame": np.array(
                            ((cosine, sine, 0), (-sine, cosine, 0), (0, 0, 1))
                        ),
                        "length": length,
                        "width": length * self.aspect,
                    }
                )
        length = (1 - self.last) * self.length
        result.append(
            {
                "id": 2 * self.pairs,
                "origin": (0, self.last * self.length, 0),
                "frame": np.eye(3),
                "length": length,
                "width": length * self.aspect,
            }
        )
        return result

    def mesh(self, segments=24, across=2, max_triangles=100_000):
        if (
            type(segments) is not int
            or type(across) is not int
            or not 2 <= segments <= 1024
            or not 1 <= across <= 32
        ):
            raise ValueError("bounded integer leaflet subdivisions required")
        count = (2 * self.pairs + 1) * (4 * across * segments - 2 * across)
        if type(max_triangles) is not int or max_triangles < 0:
            raise ValueError("nonnegative integer frond mesh budget required")
        vertices = (2 * self.pairs + 1) * (segments * (2 * across + 1) + 1)
        if count > min(max_triangles, MAX_TRIANGLES) or vertices > MAX_VERTICES:
            return None
        prototype = Blade(
            length=1,
            width=1,
            widest=0.35,
            lobes=0,
            base_fill=0.08,
            bend=0,
            fold=0,
            twist=0,
            petiole=0,
        ).mesh(segments, across)
        vertices, faces, normals, parameters, identities = [], [], [], [], []
        offset = 0
        for organ in self.organs():
            scaled = prototype["vertices"] * (organ["width"], organ["length"], 1)
            vertices.append(scaled @ organ["frame"].T + organ["origin"])
            faces.append(prototype["faces"] + offset)
            offset += len(scaled)
            normals.append(prototype["normals"] @ organ["frame"].T)
            parameters.append(prototype["parameters"])
            identities.append(np.full(len(scaled), organ["id"], dtype=np.uint32))
        result = {
            "vertices": np.concatenate(vertices),
            "faces": np.concatenate(faces),
            "normals": np.concatenate(normals),
            "parameters": np.concatenate(parameters),
            "organ_ids": np.concatenate(identities),
        }
        assert len(result["faces"]) == count
        return result
