"""Stable force recipes prepare a complete reusable spatial growth axis once."""

import hashlib
import json
from dataclasses import dataclass
from functools import cached_property

import numpy as np

from .force_fields import (
    AttractionField,
    DirectionField,
    field_from_recipe,
    point3,
    points3,
)
from .growth_integrate import constant_samples, integrate, prepared_curve
from .spine_curve import CubicSpine
from .spine_frames import unit


@dataclass(frozen=True)
class GrowthResult:
    spine: CubicSpine
    method: str
    evaluations: int
    tangent_norm_error: float
    position_error_m: float
    tangent_error_rad: float


@dataclass(frozen=True)
class GrowthSpine:
    length: float
    fields: tuple[DirectionField | AttractionField, ...] = ()
    initial_direction: tuple[float, float, float] = (0, 1, 0)

    def __post_init__(self):
        if (
            not np.isfinite(self.length)
            or not 1e-6 <= self.length <= 1000
            or not isinstance(self.fields, tuple)
            or len(self.fields) > 16
            or any(
                not isinstance(f, (DirectionField, AttractionField))
                for f in self.fields
            )
        ):
            raise ValueError(
                "bounded positive length and immutable supported growth fields required"
            )
        bound = sum(float(np.max(abs(f.rate.table))) for f in self.fields)
        if not np.isfinite(bound) or self.length * bound > 16:
            raise ValueError(
                "growth rate-times-length must stay inside the preparation range"
            )
        raw = point3(self.initial_direction)
        initial = unit(raw)
        object.__setattr__(self, "initial_direction", raw)
        object.__setattr__(self, "initial_tangent", tuple(float(v) for v in initial))

    def evaluate(self, points, fraction):
        points = points3(points)
        result = np.zeros_like(points)
        for field in self.fields:
            result += field.evaluate(points, fraction)
        return result

    def constant_vector(self):
        vector = np.zeros(3)
        for field in self.fields:
            if (
                not isinstance(field, DirectionField)
                or field.falloff is not None
                or not np.all(field.rate.table == field.rate.table[0])
            ):
                return None
            vector += np.asarray(field.unit_direction) * field.rate.table[0]
        return vector

    def prepare(self, stations=33, *, method="auto", max_evaluations=4096):
        if (
            type(stations) is not int
            or not 2 <= stations <= 65
            or type(max_evaluations) is not int
            or not 0 <= max_evaluations <= 1_000_000
            or method not in ("auto", "analytic", "DOP853", "RK45")
        ):
            raise ValueError(
                "bounded preparation stations/evaluations and declared method required"
            )
        if not max_evaluations:
            return None
        vector = self.constant_vector()
        selected = (
            ("analytic" if vector is not None else "RK45")
            if method == "auto"
            else method
        )
        distances = np.linspace(0, self.length, 4 * (stations - 1) + 1)
        if selected == "analytic":
            if vector is None:
                raise ValueError("analytic growth needs a constant direction field")
            positions, tangents = constant_samples(
                vector, np.asarray(self.initial_tangent), distances
            )
            evaluations = 0
        else:
            solved = integrate(self, distances, selected, max_evaluations)
            if solved is None:
                return None
            positions, tangents, evaluations = solved
        prepared = prepared_curve(self.length, positions, tangents)
        if prepared is None:
            return None
        spine, error, position_error, angle_error = prepared
        return GrowthResult(
            spine, selected, evaluations, error, position_error, angle_error
        )

    def recipe(self):
        return {
            "version": "growth-spine-1",
            "length": self.length,
            "fields": [f.recipe() for f in self.fields],
            "initial_direction": self.initial_direction,
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            set(recipe) != {"version", "length", "fields", "initial_direction"}
            or recipe["version"] != "growth-spine-1"
        ):
            raise ValueError("complete supported growth spine recipe required")
        return cls(
            recipe["length"],
            tuple(field_from_recipe(f) for f in recipe["fields"]),
            tuple(recipe["initial_direction"]),
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()
