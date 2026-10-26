"""Camera-independent ceiling fixtures inside existing open-floor cavities."""

import hashlib
import math

import numpy as np
import shapely


def fixture_plan(mass, spacing_m=8.0, power_w=160.0, max_candidates=4096):
    from terraced_mass_frame import local_plan
    from terraced_mass_rooms import cavities

    if not math.isfinite(spacing_m) or spacing_m < 2:
        raise ValueError("Finite ceiling grid spacing of at least2m required")
    if not math.isfinite(power_w) or power_w <= 0:
        raise ValueError("Positive finite power required")
    if type(max_candidates) is not int or max_candidates < 1:
        raise ValueError("Positive integer candidate budget required")
    local = local_plan(mass)
    rooms = [c for c in cavities(local) if c.identifier.endswith("/open-floor")]
    keepout = shapely.from_wkb(local.core.footprint_wkb).buffer(0.20)
    domains = [r.polygon.difference(keepout) for r in rooms]
    grids, candidates = [], 0
    for domain in domains:
        if domain.is_empty:
            raise ValueError("No open-floor lighting domain outside the stair core")
        lo = np.floor(np.asarray(domain.bounds[:2]) / spacing_m).astype(int)
        hi = np.ceil(np.asarray(domain.bounds[2:]) / spacing_m).astype(int)
        candidates += int(np.prod(hi - lo))
        if candidates > max_candidates:
            raise ValueError(
                "Ceiling fixture candidate budget exceeded before expansion"
            )
        grids.append((lo, hi))
    records = []
    core = mass.core
    for floor, (room, domain, (lo, hi)) in enumerate(
        zip(rooms, domains, grids, strict=True)
    ):
        positions = []
        for i in range(int(lo[0]), int(hi[0])):
            for j in range(int(lo[1]), int(hi[1])):
                x, y = (np.asarray([i, j]) + 0.5) * spacing_m
                if domain.covers(shapely.box(x - 0.4, y - 0.4, x + 0.4, y + 0.4)):
                    positions.append((str(i) + "/" + str(j), x, y, 0.8))
        if not positions:
            largest = max(shapely.get_parts(domain), key=lambda p: p.area)
            point = largest.representative_point()
            size = min(0.8, largest.boundary.distance(point) * math.sqrt(2) * 0.8)
            if size <= 0.02:
                raise ValueError("No contained ceiling fixture in the open floor")
            positions.append(("fallback", point.x, point.y, size))
        warm = (
            int(hashlib.sha256(room.identifier.encode()).hexdigest()[:8], 16) % 2 == 0
        )
        for identifier, x, y, size in positions:
            xy = (
                np.asarray(core.origin_xy)
                + x * np.asarray(core.across_xy)
                + y * np.asarray(core.along_xy)
            )
            records.append(
                {
                    "fixture_id": room.identifier + "/ceiling/" + identifier,
                    "room_id": room.identifier,
                    "storey": floor,
                    "position": [*xy.tolist(), room.high_m - 0.06],
                    "size": [size, size],
                    "power_w": power_w,
                    "colour_linear": [1.0, 0.66, 0.35] if warm else [1.0, 0.85, 0.68],
                    "origin": "Procedural ceiling grid inside the existing open floor; no room subdivision or camera input",
                }
            )
    return records


def light_state(records, mode):
    if mode not in ("all-lit", "mixed", "off"):
        raise ValueError("Unknown open-floor light state")
    return [
        r
        | {
            "enabled": mode == "all-lit"
            or mode == "mixed"
            and int(hashlib.sha256(r["room_id"].encode()).hexdigest()[:8], 16) % 3 != 0
        }
        for r in records
    ]
