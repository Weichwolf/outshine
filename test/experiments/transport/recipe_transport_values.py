"""Owned transport snapshots and explicit payload accounting for the DAG pilot."""

import hashlib
from collections.abc import Mapping
from types import MappingProxyType

import numpy as np
import shapely
from recipe_graph_data import canonical, digest


def sealed(value):
    if isinstance(value, Mapping):
        return MappingProxyType({key: sealed(item) for key, item in value.items()})
    if isinstance(value, (list, tuple)):
        return tuple(sealed(item) for item in value)
    if isinstance(value, np.ndarray):
        if value.dtype.kind not in "biuf" or not np.isfinite(value).all():
            raise ValueError("Transport snapshots require finite numeric arrays")
        return np.frombuffer(value.tobytes(), dtype=value.dtype).reshape(value.shape)
    if isinstance(value, np.generic):
        return sealed(value.item())
    if isinstance(value, (str, int, float, bool, shapely.Geometry)) or value is None:
        return value
    raise TypeError("Unsupported transport snapshot value")


def descriptor(value):
    if isinstance(value, Mapping):
        return {key: descriptor(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [descriptor(item) for item in value]
    if isinstance(value, np.ndarray):
        return {
            "dtype": value.dtype.str,
            "shape": list(value.shape),
            "sha256": hashlib.sha256(value.tobytes()).hexdigest(),
        }
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, shapely.Geometry):
        return {"wkb_hex": value.wkb_hex}
    return value


def payload_bytes(value):
    seen = set()

    def arrays(item):
        if isinstance(item, Mapping):
            return sum(arrays(child) for child in item.values())
        if isinstance(item, (tuple, list)):
            return sum(arrays(child) for child in item)
        if not isinstance(item, np.ndarray):
            return 0
        base = item
        while isinstance(base.base, np.ndarray):
            base = base.base
        if id(base) in seen:
            return 0
        seen.add(id(base))
        return base.nbytes

    return int(arrays(value) + len(canonical(descriptor(value))))


def spatial(source, representation, value):
    return dict(source.metadata) | {
        "representation": representation,
        "content_sha256": digest(descriptor(value)),
        "accounting": "Numeric array backing payload plus canonical logical descriptors; Python/GEOS/solver heap excluded",
    }


def compatible(*products):
    expected = products[0].metadata
    for product in products:
        for key in ("frame_id", "axes", "units", "height_datum"):
            if product.metadata.get(key) != expected.get(key):
                raise ValueError("Transport frame/units/height datum disagree")
        if (
            product.metadata.get("axes") != "east,north,up"
            or product.metadata.get("units") != "m"
            or product.metadata.get("height_datum")
            != "retained-source-priors-geodetic-datum-unverified"
        ):
            raise ValueError("Transport pilot requires declared local ENU metres")


def physical_facts(value):
    """Wall-clock diagnostics do not belong to a deterministic geometry product."""
    if isinstance(value, Mapping):
        return {
            key: physical_facts(item)
            for key, item in value.items()
            if key != "elapsed_ms"
        }
    if isinstance(value, (list, tuple)):
        return [physical_facts(item) for item in value]
    return value
