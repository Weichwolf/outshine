"""Vegetation-domain immutable payloads for the existing isolated recipe kernel."""

from dataclasses import dataclass
from types import MappingProxyType

from outshine_ref.contracts import immutable
from outshine_ref.vegetation.plan_arrays import immutable_array
from recipe_graph import Emission, external
from recipe_graph_data import canonical, digest

LOCAL = {
    "frame_id": "plant-local-root-z-up",
    "axes": "right,forward,up",
    "units": "m",
    "height_datum": "plant-root=0",
}


def json_emission(value, representation):
    value = immutable(value)
    return Emission(
        value,
        len(canonical(value)),
        LOCAL
        | {
            "representation": representation,
            "content_sha256": digest(value),
            "accounting": "Canonical logical JSON bytes; Python objects excluded",
        },
    )


def read_json(product, representation):
    if any(product.metadata.get(k) != v for k, v in LOCAL.items()) or (
        product.metadata.get("representation") != representation
        or product.metadata.get("content_sha256") != digest(product.value)
    ):
        raise ValueError(
            "Vegetation local frame/units/datum/representation/content disagree"
        )
    return product.value


def seed_input(seed):
    if type(seed) is not int or not 0 <= seed < 2**32:
        raise ValueError("Pilot seed requires a portable UInt32 integer")
    return external(
        "vegetation.seed/1",
        seed,
        seed,
        len(canonical(seed)),
        {"representation": "UInt32 seed; no spatial meaning"},
    )


def arrays(values):
    return MappingProxyType(
        {key: immutable_array(value) for key, value in values.items()}
    )


def array_records(values):
    import hashlib

    return {
        key: {
            "dtype": value.dtype.str,
            "shape": list(value.shape),
            "sha256": hashlib.sha256(value.tobytes()).hexdigest(),
        }
        for key, value in values.items()
    }


@dataclass(frozen=True)
class PlantDetail:
    recipe: object
    tables: object
    needles: tuple

    def descriptor(self):
        return {
            "recipe": self.recipe,
            "tables": array_records(self.tables),
            "needles": [array_records(mesh) for mesh in self.needles],
        }

    def nbytes(self):
        return len(canonical(self.descriptor())) + sum(
            a.nbytes for block in (self.tables, *self.needles) for a in block.values()
        )


def detail_emission(value):
    return Emission(
        value,
        value.nbytes(),
        LOCAL
        | {
            "representation": "Float64 local poses/prototypes; integer IDs/topology",
            "content_sha256": digest(value.descriptor()),
            "accounting": "Owned array payload plus canonical logical descriptors; native/Python heap excluded",
        },
    )
