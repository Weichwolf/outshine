"""Exact attribute representations for the existing original fragment reader."""

from types import MappingProxyType

import attribute_words
import source_fragment_inputs as fragments

KEYS = ("vertex_normal", "uv", "metres", "colour")


def compact(materials, mode="adaptive", *, bytes_most=16 * 1024**2):
    if type(bytes_most) is not int or not 0 <= bytes_most <= 32 * 1024**2:
        raise ValueError("Explicit bounded attribute product capacity required")
    values = {
        "positions": materials.geometry.positions,
        "indices": materials.geometry.triangles,
    }
    used = materials.part_ends.nbytes + materials.materials.nbytes
    if used > bytes_most:
        raise attribute_words.AttributeLimit("attribute_metadata_byte_budget")
    for key in KEYS:
        value = attribute_words.prepare(
            materials.source[key],
            mode,
            bytes_most=min(attribute_words.MAX_BYTES, bytes_most - used),
        )
        values[key] = value
        used += value.nbytes
    return fragments.MaterialInputs(
        materials.geometry,
        MappingProxyType(values),
        materials.part_ends,
        materials.materials,
    )


def prepare(field, source, mode="adaptive", *, bytes_most=16 * 1024**2):
    """Offline source preparation included; the plain snapshot is not retained."""
    return compact(fragments.prepare(field, source), mode, bytes_most=bytes_most)
