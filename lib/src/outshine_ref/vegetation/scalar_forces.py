"""Prepared scalar lookup for bounded ODE callbacks; public batch fields stay independent."""

import math
from bisect import bisect_right

import numpy as np

from .force_fields import DirectionField
from .turning_fields import CurlField, GnarlField, PlanarField, ReturnField


def compile_fields(fields, initial_direction=(0, 1, 0)):
    result = []
    for field in fields:
        table = tuple(float(v) for v in field.rate.table)
        knots = tuple(float(v) for v in np.linspace(0, 1, len(table)))
        slopes = tuple(
            (b - a) / (v - u)
            for a, b, u, v in zip(
                table[:-1], table[1:], knots[:-1], knots[1:], strict=True
            )
        )
        direction = isinstance(field, DirectionField)
        if isinstance(field, ReturnField):
            direction, coordinates = True, initial_direction
        elif isinstance(field, (CurlField, GnarlField)):
            direction, coordinates = (
                (2 if isinstance(field, CurlField) else 3),
                field.unit_axis,
            )
        elif isinstance(field, PlanarField):
            direction, coordinates = 4, field.unit_normal
        else:
            coordinates = field.unit_direction if direction else field.target
        softening2 = 0 if direction else field.softening * field.softening
        fall = field.falloff
        radial = (
            None if fall is None else (fall.centre, fall.inner, fall.outer, fall.mode)
        )
        result.append(
            (
                direction,
                coordinates,
                softening2,
                field.rate.kind,
                knots,
                table,
                slopes,
                radial,
            )
        )
    return tuple(result)


def turn_vector(kind, coords, tx, ty, tz, lateral):
    ax, ay, az = coords
    if kind == 4:
        dot = ax * tx + ay * ty + az * tz
        return tx - dot * ax, ty - dot * ay, tz - dot * az
    if kind == 2:
        if lateral is None:
            raise ValueError("moving curl requires the transported lateral vector")
        lx, ly, lz = lateral
        nx, ny, nz = ly * tz - lz * ty, lz * tx - lx * tz, lx * ty - ly * tx
        ax, ay, az = (
            ax * lx + ay * tx + az * nx,
            ax * ly + ay * ty + az * ny,
            ax * lz + ay * tz + az * nz,
        )
    return ay * tz - az * ty, az * tx - ax * tz, ax * ty - ay * tx


def scalar_force(fields, x, y, z, fraction, tx=0.0, ty=1.0, tz=0.0, lateral=None):
    fraction = min(1.0, max(0.0, fraction))
    force_x = force_y = force_z = 0.0
    for direction, coords, softening2, kind, knots, table, slopes, radial in fields:
        if kind == "maximum":
            weight = 1.0
        elif kind == "minimum":
            weight = 0.0
        elif kind == "linear":
            weight = fraction
        elif kind == "inverse_linear":
            weight = 1 - fraction
        elif fraction == 1:
            weight = table[-1]
        else:
            index = max(0, bisect_right(knots, fraction) - 1)
            weight = table[index] + slopes[index] * (fraction - knots[index])
        if radial is not None:
            centre, inner, outer, mode = radial
            dx, dy, dz = x - centre[0], y - centre[1], z - centre[2]
            distance = math.sqrt(dx * dx + dy * dy + dz * dz)
            if distance > outer:
                weight = 0.0
            elif mode != "step" and distance > inner:
                fall = 1 - (distance - inner) / (outer - inner)
                weight *= fall * fall if mode == "squared" else fall
        if direction in (2, 3, 4):
            dx, dy, dz = turn_vector(direction, coords, tx, ty, tz, lateral)
        elif direction:
            dx, dy, dz = coords
        else:
            dx, dy, dz = coords[0] - x, coords[1] - y, coords[2] - z
            weight /= math.sqrt(dx * dx + dy * dy + dz * dz + softening2)
        force_x += weight * dx
        force_y += weight * dy
        force_z += weight * dz
    return force_x, force_y, force_z
