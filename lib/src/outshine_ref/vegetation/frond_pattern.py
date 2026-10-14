"""Own frond source surfaces; stable local organ plans precede mesh detail."""

import hashlib
import json
from dataclasses import dataclass
from functools import cached_property

import numpy as np

from .blade_surface import Blade
from .frond_surface import MAX_TRIANGLES, MAX_VERTICES
from .leaflet_shape import LeafletShape


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
    leaflet_shape: LeafletShape | None = None

    def __post_init__(self):
        if self.leaflet_shape is not None and not isinstance(
            self.leaflet_shape, LeafletShape
        ):
            raise TypeError("prepared local leaflet shape required")
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
        if self.leaflet_shape is not None:
            bend, fold, _ = self.leaflet_shape.maximum
            if bend * self.aspect / 2 * (1 + fold) >= 1:
                raise ValueError("local shape/aspect must preserve a regular blade")

    def recipe(self):
        values = {
            name: getattr(self, name)
            for name in (
                "seed",
                "length",
                "pairs",
                "leaflet_length",
                "aspect",
                "angle",
                "first",
                "last",
                "variance",
            )
        }
        if self.leaflet_shape is None:
            return {"version": "pinnate-source-1", **values}
        return {
            "version": "pinnate-source-2",
            **values,
            "leaflet_shape": self.leaflet_shape.recipe(),
        }

    @classmethod
    def from_recipe(cls, recipe):
        values = dict(recipe)
        version = values.pop("version", None)
        if version == "pinnate-source-2":
            values["leaflet_shape"] = LeafletShape.from_recipe(values["leaflet_shape"])
        elif version != "pinnate-source-1" or "leaflet_shape" in values:
            raise ValueError("supported complete pinnate recipe required")
        return cls(**values)

    def shaped_organ(self, organ, parameter):
        if self.leaflet_shape is None:
            return organ
        params = self.leaflet_shape.sample(parameter, self.seed, organ["id"])
        cosine, sine = organ["frame"][1, 1], abs(organ["frame"][1, 0])
        margin = self.aspect / 2 * (1 + abs(params[1])) * (cosine + sine)
        limit = (1 - parameter) * self.length / (cosine + margin)
        if margin > 0:
            limit = min(limit, parameter * self.length / margin)
        length = min(organ["length"], limit)
        return organ | {
            "length": length,
            "width": length * self.aspect,
            "shape": params,
        }

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
                    self.shaped_organ(
                        {
                            "id": identity,
                            "origin": (0, self.length * parameter, 0),
                            "frame": np.array(
                                ((cosine, sine, 0), (-sine, cosine, 0), (0, 0, 1))
                            ),
                            "length": length,
                            "width": length * self.aspect,
                        },
                        parameter,
                    )
                )
        length = (1 - self.last) * self.length
        result.append(
            self.shaped_organ(
                {
                    "id": 2 * self.pairs,
                    "origin": (0, self.last * self.length, 0),
                    "frame": np.eye(3),
                    "length": length,
                    "width": length * self.aspect,
                },
                self.last,
            )
        )
        return result

    def leaflet_prototype(self, shape, segments, across):
        bend, fold, twist = (0, 0, 0) if shape is None else shape
        return Blade(
            length=1,
            width=1 if shape is None else self.aspect,
            widest=0.35,
            lobes=0,
            base_fill=0.08,
            bend=bend,
            fold=fold,
            twist=twist,
            petiole=0,
        ).mesh(segments, across)

    @property
    def source_extent(self):
        length = max(
            self.leaflet_length * (1 + self.variance), self.length * (1 - self.last)
        )
        fold = 0 if self.leaflet_shape is None else self.leaflet_shape.maximum[1]
        return length * (1 + self.aspect / 2 * (1 + fold))

    def mesh(self, segments=24, across=2, max_triangles=100_000, *, method="shared"):
        if method not in ("shared", "individual"):
            raise ValueError("declared local prototype method required")
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
        vertices, faces, normals, parameters, identities = [], [], [], [], []
        prototypes = {}
        offset = 0
        for organ in self.organs():
            shape = organ.get("shape")
            if method == "individual":
                current = self.leaflet_prototype(shape, segments, across)
            else:
                if shape not in prototypes:
                    prototypes[shape] = self.leaflet_prototype(shape, segments, across)
                current = prototypes[shape]
            if self.leaflet_shape is not None:
                scaled = current["vertices"] * organ["length"]
            else:
                scaled = current["vertices"] * (organ["width"], organ["length"], 1)
            vertices.append(scaled @ organ["frame"].T + organ["origin"])
            faces.append(current["faces"] + offset)
            offset += len(scaled)
            normals.append(current["normals"] @ organ["frame"].T)
            parameters.append(current["parameters"])
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
