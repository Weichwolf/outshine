"""Bounded derived road axes; original knots and endpoint frames stay separate."""

from numbers import Real

import numpy as np
import shapely

from .clothoid_turns import fit_symmetric_turns, sample_symmetric_turns


def settings(recipe):
    required = {"model", "radius_m", "error_m", "end_guard_m", "maximum_shift_m"}
    if set(recipe) != required or recipe["model"] not in ("circle", "clothoid"):
        raise ValueError("explicit complete horizontal curve recipe required")
    if any(
        not isinstance(recipe[k], Real) or isinstance(recipe[k], (bool, np.bool_))
        for k in required - {"model"}
    ):
        raise ValueError("numeric horizontal curve limits required")
    values = np.array([recipe[k] for k in sorted(required - {"model"})], float)
    if not np.isfinite(values).all() or np.any(values <= 0):
        raise ValueError("positive finite horizontal curve limits required")
    if not 4 <= recipe["radius_m"] <= 1000 or not 1e-4 <= recipe["error_m"] <= 0.1:
        raise ValueError("bounded horizontal radius and chord error required")
    if recipe["end_guard_m"] > 1000 or recipe["maximum_shift_m"] > 1000:
        raise ValueError("bounded horizontal approach and displacement required")


def curves(xy, recipe):
    delta = np.diff(xy, axis=0)
    spans = np.linalg.norm(delta, axis=1)
    axis = delta / spans[:, None]
    cross = axis[:-1, 0] * axis[1:, 1] - axis[:-1, 1] * axis[1:, 0]
    turn = np.arctan2(cross, np.einsum("ij,ij->i", axis[:-1], axis[1:]))
    if np.any(abs(turn) >= np.deg2rad(150)):
        raise ValueError("returning road corner needs a separate recipe")
    chosen = np.flatnonzero(abs(turn) > 1e-8)
    headings = np.arctan2(axis[:, 1], axis[:, 0])
    cuts = np.zeros(len(xy))
    plans = []
    for i in chosen:
        unit = fit_symmetric_turns(
            xy[i + 1],
            np.ones(1),
            headings[i : i + 1],
            headings[i + 1 : i + 2],
            transition_fraction=0.25,
        )
        factor = (
            float(unit["maximum_curvature_per_m"][0])
            if recipe["model"] == "clothoid"
            else np.tan(abs(turn[i]) / 2)
        )
        cuts[i + 1] = recipe["radius_m"] * factor
        plans.append((int(i + 1), float(turn[i]), float(headings[i])))
    available = spans.copy()
    available[[0, -1]] -= recipe["end_guard_m"]
    if np.any(cuts[:-1] + cuts[1:] > available):
        raise ValueError("horizontal radius leaves insufficient original straight")
    return cuts, plans


def corner(xy, index, angle, heading, cut, recipe):
    incoming = np.array([np.cos(heading), np.sin(heading)])
    outgoing = np.array([np.cos(heading + angle), np.sin(heading + angle)])
    start, end = xy[index] - cut * incoming, xy[index] + cut * outgoing
    if recipe["model"] == "clothoid":
        fit = fit_symmetric_turns(
            xy[index],
            np.array([cut]),
            np.array([heading]),
            np.array([heading + angle]),
            transition_fraction=0.25,
        )
        if not fit["valid"][0]:
            raise ValueError("horizontal curve endpoint closure failed")
        length = float(fit["length_m"][0])
        curvature = float(fit["maximum_curvature_per_m"][0])
    else:
        fit = None
        length = abs(angle) * recipe["radius_m"]
        curvature = 1 / recipe["radius_m"]
    count = max(2, int(np.ceil(length * np.sqrt(curvature / (8 * recipe["error_m"])))))
    if count > 240:
        raise ValueError("horizontal curve sample budget exhausted")
    stations = np.linspace(0, 1, count + 1)
    if fit is not None:
        points = sample_symmetric_turns(fit, stations)[0][0]
        closure = float(fit["closure_error_m"][0])
    else:
        normal = np.array([-incoming[1], incoming[0]]) * np.sign(angle)
        center = start + recipe["radius_m"] * normal
        radial = start - center
        phase = angle * stations
        points = (
            center
            + np.c_[
                radial[0] * np.cos(phase) - radial[1] * np.sin(phase),
                radial[0] * np.sin(phase) + radial[1] * np.cos(phase),
            ]
        )
        closure = float(np.linalg.norm(points[-1] - end))
    if max(np.linalg.norm(points[0] - start), np.linalg.norm(points[-1] - end)) > 1e-7:
        raise ValueError("actual horizontal samples miss endpoint poses")
    points[0], points[-1] = start, end
    return points, {
        "original_knot": index,
        "turn_rad": angle,
        "incoming_heading_rad": heading,
        "trim_m": cut,
        "length_m": length,
        "maximum_curvature_per_m": curvature,
        "endpoint_curvature_per_m": 0.0 if fit is not None else curvature,
        "segments": count,
        "chord_bound_m": curvature * (length / count) ** 2 / 8,
        "closure_error_m": closure,
    }


def plan_horizontal_axis(original, recipe):
    """Return an owned read-only derived axis and fresh explicit construction notes."""
    settings(recipe)
    xy = np.asarray(original, float)
    if xy.ndim != 2 or xy.shape[1] != 2 or not 2 <= len(xy) <= 128:
        raise ValueError("bounded original horizontal axis required")
    if (
        not np.isfinite(xy).all()
        or abs(xy).max() > 1e6
        or np.any(np.linalg.norm(np.diff(xy, axis=0), axis=1) < 1e-6)
    ):
        raise ValueError("finite distinct horizontal source knots required")
    cuts, plans = curves(xy, recipe)
    if np.linalg.norm(xy[0] - xy[-1]) < 1e-6:
        raise ValueError("closed horizontal axes need a separate join recipe")
    parts, notes = [xy[:1]], []
    for index, angle, heading in plans:
        points, note = corner(xy, index, angle, heading, cuts[index], recipe)
        parts.append(points)
        notes.append(note)
    parts.append(xy[-1:])
    if sum(len(p) for p in parts) > 256:
        raise ValueError("derived road-axis sample budget exhausted")
    result = np.vstack(parts)
    line = shapely.LineString(result)
    if not line.is_simple or not shapely.LineString(xy).buffer(
        recipe["maximum_shift_m"]
    ).covers(line):
        raise ValueError("horizontal curve loops or leaves allowed original corridor")
    result.flags.writeable = False
    return result, {
        "recipe": dict(recipe),
        "corners": notes,
        "original_knots": len(xy),
        "derived_knots": len(result),
        "scope": "Derived horizontal axis only; actual finite road/vehicle/terrain admission required.",
    }
