"""Resolve mapped façade colours on building outlines and their visible parts."""

from string import hexdigits

from PIL import ImageColor

from outshine.buildings.building_envelope import outline_matches
from outshine.buildings.structures import suppressed


def mapped_colour(properties):
    value = properties.get("colour")
    if not isinstance(value, str):
        return None
    value = value.strip()
    if len(value) == 6 and all(channel in hexdigits for channel in value):
        value = "#" + value
    try:
        rgb = ImageColor.getrgb(value)
    except ValueError:
        return None
    if len(rgb) == 4 and rgb[3] != 255:
        return None
    if not all(0 <= channel <= 255 for channel in rgb[:3]):
        return None
    return "#" + bytes(rgb[:3]).hex()


def facade_colours(geometries, properties):
    colours = [mapped_colour(p) for p in properties]
    parents = [i for i, p in enumerate(properties) if colours[i] and suppressed(p)]
    children = [
        i for i, p in enumerate(properties) if not colours[i] and not suppressed(p)
    ]
    owner = {}
    for child, parent, rank in outline_matches(geometries, parents, children):
        prior = owner.get(child)
        if prior is None or rank < prior[0]:
            owner[child] = (rank, colours[parent])
        elif rank == prior[0] and colours[parent] != prior[1]:
            owner[child] = (rank, None)
    for child, (_, colour) in owner.items():
        colours[child] = colour
    return colours


def wall_material(base, colour):
    return f"{base} {colour}" if colour else base
