"""Shared land cover classes and raster coverage for mapped terrain."""

import numpy as np
from PIL import Image, ImageDraw

COVER_LAYERS = ("landuse", "landcover")
CROP_VARIANTS = ("green crop",) * 4 + ("dry crop",) * 3 + ("field soil",)
SETTLEMENT = {
    "commercial",
    "industrial",
    "neighbourhood",
    "quarter",
    "residential",
    "retail",
    "suburb",
}
PARCEL_SURFACES = {
    "settlement": ((0.145, 0.126, 0.113), 0.94),
    "scrub": ((0.075, 0.119, 0.048), 0.91),
    "wetland": ((0.062, 0.103, 0.069), 0.93),
    "green crop": ((0.086, 0.137, 0.041), 0.92),
    "dry crop": ((0.171, 0.164, 0.079), 0.94),
    "field soil": ((0.142, 0.104, 0.066), 0.95),
    "vineyard": ((0.102, 0.128, 0.058), 0.93),
    "orchard": ((0.070, 0.111, 0.045), 0.92),
    "quarry": ((0.242, 0.211, 0.172), 0.93),
    "sand": ((0.285, 0.237, 0.161), 0.96),
    "scree": ((0.199, 0.186, 0.162), 0.94),
    "bare rock": ((0.244, 0.233, 0.213), 0.91),
    "ice": ((0.544, 0.635, 0.678), 0.82),
}
COVER_ORDER = (
    "settlement",
    "grass",
    "scrub",
    "wetland",
    "green crop",
    "dry crop",
    "field soil",
    "vineyard",
    "orchard",
    "meadow",
    "wood",
    "quarry",
    "sand",
    "scree",
    "bare rock",
    "ice",
)
COVER_PRIORITY = {name: i for i, name in enumerate(COVER_ORDER)}


def cover_material(layer, tags, feature_id=None):
    kind, detail = tags.get("class"), tags.get("subclass")
    if layer == "landuse":
        if kind in SETTLEMENT:
            return "settlement"
        return "quarry" if kind == "quarry" else None
    if kind == "farmland":
        if detail in ("vineyard", "orchard"):
            return detail
        if not feature_id:
            return "green crop"
        variation = ((int(feature_id) * 0x9E3779B1) & 0xFFFFFFFF) >> 29
        return CROP_VARIANTS[variation]
    if kind == "rock":
        return "scree" if detail == "scree" else "bare rock"
    if kind == "grass":
        if detail in ("scrub", "heath"):
            return "scrub"
        return "meadow" if detail == "meadow" else "grass"
    if kind in ("wood", "wetland", "sand", "ice"):
        return kind
    return None


def paint_cover(image, perimeter, holes, code):
    origin = np.maximum(0, perimeter.min(axis=0))
    stop = np.minimum(image.size, perimeter.max(axis=0) + 1)
    if np.any(stop <= origin):
        return
    if not holes:
        ImageDraw.Draw(image).polygon([tuple(p) for p in perimeter], fill=code)
        return
    mask = Image.new("L", tuple(stop - origin))
    paint = ImageDraw.Draw(mask)
    paint.polygon([tuple(p) for p in perimeter - origin], fill=255)
    for hole in holes:
        paint.polygon([tuple(p) for p in hole - origin], fill=0)
    image.paste(code, (*origin, *stop), mask)
