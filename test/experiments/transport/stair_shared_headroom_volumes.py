"""Only the missing upper band; existing physical volumes and planes stay intact."""

from fractions import Fraction
from math import nextafter

import shapely
from stair_module_inputs import reference_profiles
from stair_occupancy_geometry import AffineOccupancy


def physical_clearance(keys, sources):
    profiles = reference_profiles()
    return max(
        profiles[sources[k]["properties"].get("subclass") or "unclassified"][
            "clearanceM"
        ]
        for k in keys
    )


def upper_band(base, domain, extra_m, owner_id, *, full=False):
    if not 0 < extra_m <= 4.5:
        raise ValueError("positive bounded extra headroom required")
    target = Fraction(base.upper_plane_m[0]) + Fraction(extra_m)
    upper = float(target)
    if Fraction(upper) < target:
        upper = nextafter(upper, float("inf"))
    return AffineOccupancy(
        owner_id,
        domain,
        "transport.borrowed-headroom",
        base.origin_xy_m,
        base.lower_plane_m if full else base.upper_plane_m,
        (upper, *base.upper_plane_m[1:]),
    )


def supplement(rows, owners, chosen, sources, *, full=False):
    by_id = {r.owner_id: (r, keys) for r, keys in zip(rows, owners, strict=True)}
    extra, origins, records = [], [], []
    candidates, intersections = 0, 0
    for chosen_row in chosen:
        old, domain = chosen_row["old_asset"], chosen_row["domain"]
        indices = chosen_row["indices"]
        candidates += len(indices)
        if candidates > 32768:
            raise ValueError("whole borrowed headroom facet budget exhausted")
        selected = old.shapes[indices]
        pieces = shapely.intersection(selected, domain)
        intersections += len(indices)
        if not shapely.is_valid(pieces).all():
            raise ValueError("borrowed headroom clipping is unresolved")
        for face, part in zip(indices, pieces, strict=True):
            if part.is_empty or part.area == 0:
                continue
            if part.geom_type not in ("Polygon", "MultiPolygon"):
                raise ValueError("borrowed headroom has nonpolygonal positive pieces")
            original_id = f"{old.identity}:face:{face}"
            base, keys = by_id[original_id]
            prior = physical_clearance(keys, sources)
            required = chosen_row["required_m"]
            if required <= prior:
                continue
            key = chosen_row["source_id"]
            owner_id = original_id + ":headroom:" + key
            extra.append(upper_band(base, part, required - prior, owner_id, full=full))
            origins.append(tuple(dict.fromkeys((key, *keys))))
            records.append(
                {
                    "owner_id": owner_id,
                    "physical_owner_id": original_id,
                    "logical_source_id": key,
                    "physical_source_ids": list(keys),
                    "base_clearance_m": prior,
                    "required_clearance_m": required,
                    "asset_sha256": old.identity,
                    "asset_top_face_index": int(face),
                }
            )
    if len(rows) + len(extra) > 8192 or len({r.owner_id for r in extra}) != len(extra):
        raise ValueError("bounded unique supplemental headroom ledger required")
    return (
        tuple(extra),
        tuple(origins),
        records,
        {
            "declared_contacts": len(chosen),
            "old_owner_facets": candidates,
            "facet_intersections": intersections,
            "additional_volumes": len(extra),
            "geometry_array_copies_bytes": 0,
            "scope": "Existing immutable facet/plane objects reused; clipped GEOS geometry allocates. Internal GEOS copied bytes unmeasured.",
        },
    )
