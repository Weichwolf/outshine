"""Finite geometry-only window templates; independent of generators and GEOS."""

import json
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

import numpy as np

MAX_INSTANCES = 8192
FIELDS = ("vertices", "faces", "materials", "parts")


def frozen(value):
    return np.frombuffer(value.tobytes(), dtype=value.dtype).reshape(value.shape)


def geometry(value):
    if set(value) != set(FIELDS) or any(
        not isinstance(value[k], np.ndarray) for k in FIELDS
    ):
        raise ValueError("Closed geometry requires four native array fields")
    p, f, m, parts = (value[k] for k in FIELDS)
    if (
        p.dtype != np.float64
        or p.ndim != 2
        or p.shape[1] != 3
        or not 0 < len(p) <= 1024
    ):
        raise ValueError("Invalid closed-template vertices")
    if not np.isfinite(p).all() or np.max(np.abs(p)) > 1e8:
        raise ValueError("Invalid closed-template coordinates")
    if (
        f.dtype.kind not in "iu"
        or f.ndim != 2
        or f.shape[1] != 3
        or not 0 < len(f) <= 512
    ):
        raise ValueError("Invalid closed-template topology")
    if f.min() < 0 or f.max() >= len(p):
        raise ValueError("Closed-template index outside its vertex array")
    if (
        m.shape != (len(f),)
        or m.dtype.kind not in "iu"
        or not np.isin(m, (3, 10, 11)).all()
    ):
        raise ValueError("Unsupported closed-window material roles")
    if parts.shape != (len(f),) or parts.dtype.kind not in "iu" or parts.min() < 0:
        raise ValueError("Invalid closed-template part identities")
    return MappingProxyType({key: frozen(value[key]) for key in FIELDS})


@dataclass(frozen=True)
class WindowGeometry:
    source_id: str
    recipe: str
    identifiers: tuple
    frame: np.ndarray
    placements: np.ndarray
    indices: np.ndarray
    templates: tuple

    def __post_init__(self):
        if not isinstance(self.identifiers, (tuple, list)):
            raise TypeError("Window identities require a bounded collection")
        ids = tuple(self.identifiers)
        if (
            not isinstance(self.source_id, str)
            or not self.source_id
            or len(self.source_id) > 1024
        ):
            raise ValueError("Invalid source identity")
        if self.recipe not in ("cascade", "paired-wings") or len(ids) > MAX_INSTANCES:
            raise ValueError("Unsupported recipe or window count")
        if any(not isinstance(i, str) or not i or len(i) > 1024 for i in ids) or len(
            set(ids)
        ) != len(ids):
            raise ValueError("Invalid or duplicate window identities")
        for value, shape in ((self.frame, (3, 2)), (self.placements, (len(ids), 5))):
            if (
                not isinstance(value, np.ndarray)
                or value.dtype != np.float64
                or value.shape != shape
            ):
                raise ValueError("Invalid window frame or placement layout")
            if not np.isfinite(value).all() or np.any(np.abs(value) > 1e8):
                raise ValueError("Invalid window placement coordinates")
        if np.any(np.abs(np.linalg.norm(self.placements[:, 2:4], axis=1) - 1) > 1e-8):
            raise ValueError("Window tangents must be unit vectors")
        if (
            not isinstance(self.indices, np.ndarray)
            or self.indices.dtype.kind not in "iu"
            or self.indices.shape != (len(ids),)
        ):
            raise ValueError("Invalid template indices")
        if len(self.templates) > len(ids) or (
            len(ids)
            and (
                not self.templates
                or self.indices.min() < 0
                or self.indices.max() >= len(self.templates)
            )
        ):
            raise ValueError("Template index outside the shared table")
        object.__setattr__(self, "identifiers", ids)
        object.__setattr__(self, "frame", frozen(self.frame))
        object.__setattr__(self, "placements", frozen(self.placements))
        object.__setattr__(self, "indices", frozen(self.indices.astype(np.uint32)))
        object.__setattr__(
            self, "templates", tuple(geometry(t) for t in self.templates)
        )
        object.__setattr__(
            self, "_ordinals", MappingProxyType({name: i for i, name in enumerate(ids)})
        )

    def emit(self, identifier):
        row = self._ordinals[identifier]
        template = self.templates[int(self.indices[row])]
        x, y, tx, ty, z = self.placements[row]
        rotation = np.array([[tx, -ty, 0.0], [ty, tx, 0.0], [0.0, 0.0, 1.0]])
        local = template["vertices"] @ rotation.T + [x, y, z]
        points = local.copy()
        points[:, :2] = (
            self.frame[0] + local[:, :1] * self.frame[1] + local[:, 1:2] * self.frame[2]
        )
        if not np.isfinite(points).all() or np.max(np.abs(points)) > 1e8:
            raise ValueError("Window emission outside finite coordinate contract")
        return MappingProxyType(dict(template) | {"vertices": frozen(points)})

    @property
    def array_bytes(self):
        return sum(a.nbytes for a in (self.frame, self.placements, self.indices)) + sum(
            v.nbytes for t in self.templates for v in t.values()
        )


def save(asset, directory):
    """Experimental trusted-input snapshot, not a native cache format."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(
        directory / "placements.npz",
        frame=asset.frame,
        placements=asset.placements,
        indices=asset.indices,
    )
    for i, template in enumerate(asset.templates):
        np.savez_compressed(directory / f"template-{i}.npz", **template)
    (directory / "asset.json").write_text(
        json.dumps(
            {
                "format": 1,
                "source_id": asset.source_id,
                "recipe": asset.recipe,
                "identifiers": asset.identifiers,
                "templates": len(asset.templates),
            },
            indent=2,
        )
        + "\n"
    )


def load(directory, expected_source_id, expected_recipe):
    directory = Path(directory)
    info = json.loads((directory / "asset.json").read_text())
    if (
        type(info["format"]) is not int
        or info["format"] != 1
        or info["source_id"] != expected_source_id
        or info["recipe"] != expected_recipe
    ):
        raise ValueError("Window snapshot source or format mismatch")
    count = info["templates"]
    if not isinstance(info["identifiers"], list) or (
        type(count) is not int
        or not 0 <= count <= MAX_INSTANCES
        or len(info["identifiers"]) > MAX_INSTANCES
    ):
        raise ValueError("Window snapshot exceeds its finite capacity")
    with np.load(directory / "placements.npz", allow_pickle=False) as data:
        frame, placements, indices = (
            data[k] for k in ("frame", "placements", "indices")
        )
    templates = []
    for i in range(count):
        with np.load(directory / f"template-{i}.npz", allow_pickle=False) as data:
            templates.append({key: data[key] for key in FIELDS})
    return WindowGeometry(
        info["source_id"],
        info["recipe"],
        tuple(info["identifiers"]),
        frame,
        placements,
        indices,
        tuple(templates),
    )
