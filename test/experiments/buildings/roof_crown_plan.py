"""Source-contained roof programmes, partitioned in 2D before any mesh work."""

from dataclasses import dataclass

import numpy as np
import shapely
from building_layout import frame
from shapely.affinity import affine_transform


@dataclass(frozen=True)
class Region:
    name: str
    footprint_wkb: bytes
    low_m: float
    high_m: float
    material: int
    high_edge_wkb: bytes = b""
    low_edge_wkb: bytes = b""


@dataclass(frozen=True)
class Crown:
    footprint_wkb: bytes
    top_m: float
    deck_m: float
    guard_m: float
    regions: tuple[Region, ...]
    kind: str
    service_reason: str


def service_rectangle(deck):
    """Bounded search, not a largest-inscribed-rectangle or optimum claim."""
    host = deck.buffer(-1.5, join_style="mitre")
    for component in sorted(shapely.get_parts(host), key=lambda p: (-p.area, p.wkb)):
        if component.geom_type != "Polygon" or component.is_empty:
            continue
        forward, reverse = frame(component)
        local = affine_transform(component, forward)
        a, b, c, d = local.bounds
        for scale in (1.0, 0.75, 0.5):
            length = min(18.0, (c - a) * 0.45) * scale
            width = min(8.0, (d - b) * 0.28) * scale
            if length < 6.0 or width < 3.0:
                continue
            for u, v in (
                (0.5, 0.5),
                (0.25, 0.5),
                (0.75, 0.5),
                (0.5, 0.25),
                (0.5, 0.75),
            ):
                x, y = a + u * (c - a), b + v * (d - b)
                rectangle = shapely.box(
                    x - length / 2, y - width / 2, x + length / 2, y + width / 2
                )
                if local.covers(rectangle) and rectangle.area <= deck.area * 0.3:
                    return affine_transform(rectangle, reverse)
    return None


def plan(polygon, top_m, kind, *, body_min_m=2.64):
    if (
        not isinstance(polygon, shapely.Polygon)
        or polygon.is_empty
        or not polygon.is_valid
        or not np.isfinite(shapely.get_coordinates(polygon)).all()
        or not np.isfinite([top_m, body_min_m]).all()
        or body_min_m <= 0
        or kind not in ("parapet", "parapet-service")
    ):
        raise ValueError("Crown requires a finite valid polygon, heights and programme")
    polygon = shapely.orient_polygons(polygon)
    guard = 1.1
    offset = polygon.buffer(-0.16, join_style="mitre")
    deck = polygon.buffer(-0.28, join_style="mitre")
    if (
        deck.is_empty
        or deck.area < 10.0
        or not polygon.covers(offset)
        or not offset.covers(deck)
    ):
        raise ValueError("Insufficient contained deck for a roof crown")
    service = service_rectangle(deck) if kind == "parapet-service" else None
    reason = "not requested"
    if kind == "parapet-service":
        reason = (
            "bounded contained rectangle"
            if service is not None
            else "no contained service rectangle; parapet fallback"
        )
    rise = min(3.0, top_m - body_min_m) if service is not None else guard
    if rise < guard or top_m - rise < body_min_m:
        raise ValueError("Source height leaves no safe body below the crown")
    if service is not None and rise < guard + 0.5:
        service = None
        rise = guard
        reason = "insufficient service rise; parapet fallback"
    datum = top_m - rise
    edge_top = top_m if service is None else datum + guard
    regions = [
        Region("coping", polygon.difference(offset).wkb, edge_top, edge_top, 10),
        Region(
            "guard-inner",
            offset.difference(deck).wkb,
            datum,
            edge_top,
            10,
            offset.boundary.wkb,
            deck.boundary.wkb,
        ),
    ]
    if service is None:
        regions.append(Region("deck", deck.wkb, datum, datum, 9))
        kind = "parapet"
    else:
        cap = service.buffer(-0.12, join_style="mitre")
        regions.extend(
            (
                Region("deck", deck.difference(service).wkb, datum, datum, 9),
                Region(
                    "service-side",
                    service.difference(cap).wkb,
                    datum,
                    top_m,
                    10,
                    cap.boundary.wkb,
                    service.boundary.wkb,
                ),
                Region("service-cap", cap.wkb, top_m, top_m, 9),
            )
        )
    result = Crown(
        polygon.wkb, float(top_m), float(datum), guard, tuple(regions), kind, reason
    )
    validate_partition(result)
    return result


def validate_partition(crown):
    source = shapely.from_wkb(crown.footprint_wkb)
    regions = [shapely.from_wkb(r.footprint_wkb) for r in crown.regions]
    if any(
        g.is_empty or not g.is_valid or g.geom_type not in ("Polygon", "MultiPolygon")
        for g in regions
    ):
        raise ValueError("Crown contains an empty or invalid region")
    if source.symmetric_difference(shapely.union_all(regions)).area > 1e-8:
        raise ValueError("Crown partition fails complete source coverage")
    if any(
        a.intersection(b).area > 1e-8
        for i, a in enumerate(regions)
        for b in regions[i + 1 :]
    ):
        raise ValueError("Crown regions overlap")
