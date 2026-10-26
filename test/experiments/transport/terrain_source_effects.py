"""Explicit instance-qualified terrain roles referring to actual body undersides."""

import math
from collections.abc import Mapping

import numpy as np

_NAMES = ("Unspecified", "GroundContact", "OverheadClearance", "NoEarthwork")
_GROUND, _CLEARANCE, _NO_EARTHWORK = 1, 2, 3


def _finite_nonnegative(value):
    if not isinstance(value, (int, float, np.integer, np.floating)) or isinstance(
        value, (bool, np.bool_)
    ):
        return False
    try:
        return math.isfinite(value) and value >= 0
    except OverflowError:
        return False


def _source_roles(rows, declarations):
    ids = [row["source_id"] for row in rows]
    if not isinstance(declarations, Mapping):
        raise TypeError("explicit source-instance terrain declarations required")
    if not 1 <= len(ids) <= 64 or None in ids or len(set(ids)) != len(ids):
        raise ValueError("bounded unique source-instance identities required")
    missing, foreign = set(ids) - set(declarations), set(declarations) - set(ids)
    if missing or foreign:
        raise ValueError(
            f"terrain role instance mismatch: missing={sorted(missing, key=repr)!r}, foreign={sorted(foreign, key=repr)!r}"
        )
    records, payload = [], []
    for source_id in ids:
        declaration = declarations[source_id]
        if not isinstance(declaration, Mapping):
            raise TypeError(f"explicit terrain declaration required for {source_id}")
        name = declaration.get("effect", "Unspecified")
        if not isinstance(name, str) or name not in _NAMES or name == "Unspecified":
            raise ValueError(
                f"explicit qualified terrain effect required for {source_id}"
            )
        effect = _NAMES.index(name)
        key = declaration.get("contact_key", 0)
        clearance, relief = (
            declaration.get("clearance_m", 0),
            declaration.get("relief_m", 0),
        )
        recipe = declaration.get("recipe", "")
        if (
            not isinstance(key, (int, np.integer))
            or isinstance(key, bool)
            or not 0 <= key < 2**64
            or (effect == _GROUND and key == 0)
            or not all(_finite_nonnegative(value) for value in (clearance, relief))
            or not isinstance(recipe, str)
            or not recipe.strip()
        ):
            raise ValueError(
                f"valid explicit terrain recipe parameters required for {source_id}"
            )
        records.append((effect, int(key), float(clearance), float(relief)))
        payload.append(
            {
                "source_id": source_id,
                "effect": name,
                "contact_key": int(key),
                "clearance_m": float(clearance),
                "relief_m": float(relief),
                "origin": "generator_declaration",
                "recipe": recipe,
            }
        )
    return records, payload


def _resolve_mask(mask, sources, face):
    result = (_NO_EARTHWORK, 0, 0.0, 0.0)
    while mask:
        bit = mask & -mask
        effect, key, clearance, relief = sources[bit.bit_length() - 1]
        mask ^= bit
        current, old_key, old_clearance, old_relief = result
        if effect == _GROUND:
            if current == _GROUND and old_key != key:
                raise ValueError(
                    f"conflicting ground contact keys on actual body face {face}"
                )
            result = (
                _GROUND,
                key,
                0.0,
                max(old_relief, relief) if current == _GROUND else relief,
            )
        elif effect == _CLEARANCE and current != _GROUND:
            result = (
                _CLEARANCE,
                0,
                max(old_clearance, clearance),
                max(old_relief, relief),
            )
    return result


def qualify_effects(
    plan, body_data, declarations, *, max_faces=8192, cancelled=None, reuse_masks=True
):
    """Require every local source intent; never infer it from provider IDs or heights."""
    if cancelled is not None and cancelled():
        raise ValueError("terrain role planning cancelled")
    sources, payload = _source_roles(plan["rows"], declarations)
    points, faces, masks = (
        body_data[k] for k in ("body_vertices", "bottom_faces", "face_sources")
    )
    if (
        not all(isinstance(a, np.ndarray) for a in (points, faces, masks))
        or points.ndim != 2
        or points.shape[1] != 3
        or faces.ndim != 2
        or faces.shape[1] != 3
        or masks.ndim != 1
        or len(masks) != len(faces)
        or not np.issubdtype(faces.dtype, np.integer)
        or not np.issubdtype(masks.dtype, np.integer)
        or (len(faces) and (faces.min() < 0 or faces.max() >= len(points)))
    ):
        raise ValueError("actual indexed body underside and source masks required")
    if len(faces) > max_faces:
        raise ValueError("terrain role face budget exhausted")
    cache, active, effects, keys, offsets, reliefs = {}, [], [], [], [], []
    incidences, resolved_bits = 0, 0
    for face, value in enumerate(masks):
        if cancelled is not None and cancelled():
            raise ValueError("terrain role planning cancelled")
        mask = int(value)
        if mask <= 0 or mask >> len(sources):
            raise ValueError(f"body face {face} has an invalid source-instance mask")
        incidences += mask.bit_count()
        if not reuse_masks or mask not in cache:
            try:
                resolved = _resolve_mask(mask, sources, face)
            except ValueError as error:
                ids = [
                    r["source_id"]
                    for i, r in enumerate(plan["rows"])
                    if mask & (1 << i)
                ]
                raise ValueError(f"{error}: sources={ids!r}") from error
            resolved_bits += mask.bit_count()
            if reuse_masks:
                cache[mask] = resolved
        else:
            resolved = cache[mask]
        effect, key, clearance, relief = resolved
        if effect == _NO_EARTHWORK:
            continue
        active.append(face)
        effects.append(effect)
        keys.append(key)
        offsets.append(-clearance if effect == _CLEARANCE else 0.0)
        reliefs.append(relief)
    arrays = {
        "body_cap_face_ids": np.asarray(active, dtype=np.uint32),
        "effects": np.asarray(effects, dtype=np.uint8),
        "contact_keys": np.asarray(keys, dtype=np.uint64),
        "height_offsets_m": np.asarray(offsets, dtype=np.float64),
        "relief_m": np.asarray(reliefs, dtype=np.float64),
    }
    return {
        "sources": payload,
        **arrays,
        "surface_array": "body_vertices",
        "face_array": "bottom_faces",
        "height_policy": {
            "GroundContact": "underside_contact",
            "OverheadClearance": "ceiling_only",
            "NoEarthwork": "unchanged",
        },
        "work": {
            "sources": len(sources),
            "faces": len(faces),
            "owner_incidences": incidences,
            "unique_masks": len(cache) if reuse_masks else None,
            "resolved_owner_bits": resolved_bits,
            "no_earthwork_faces": len(faces) - len(active),
            "selected_array_bytes": sum(a.nbytes for a in arrays.values()),
            "point_bytes_copied": 0,
        },
        "scope": "Explicit source intent and underside references only; no DEM deformation, tunnel opening, native or World admission.",
    }
