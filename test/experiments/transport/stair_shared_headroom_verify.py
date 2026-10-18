"""Read every extra interval against immutable source facets and exact binary planes."""

from fractions import Fraction

from stair_occupancy_geometry import plane_value
from stair_shared_headroom import bindings
from stair_shared_headroom_volumes import physical_clearance


def verify_shared(
    rows, data, assets, modules, sources, first, report, *, prepared_contacts=()
):
    declared = bindings(assets, modules, prepared_contacts=prepared_contacts)
    base = {r.owner_id: (r, d) for r, d in zip(rows[:first], data[:first], strict=True)}
    actual_records, count, maximum = [], first, Fraction(0)
    for contact in declared:
        asset = contact["old_asset"]
        for face in contact["indices"]:
            shape = asset.shapes[face].intersection(contact["domain"])
            if shape.is_empty:
                continue
            if shape.area == 0:
                if shape.geom_type not in (
                    "Point",
                    "MultiPoint",
                    "LineString",
                    "MultiLineString",
                ):
                    raise ValueError(
                        "nonempty borrowed headroom area is unrepresentable"
                    )
                continue
            identity = f"{asset.identity}:face:{face}"
            physical, original = base[identity]
            old_height = physical_clearance(original["source_ids"], sources)
            required = contact["required_m"]
            if required <= old_height:
                continue
            key = contact["source_id"]
            owner = identity + ":headroom:" + key
            if count >= len(rows):
                raise ValueError("missing whole borrowed source headroom")
            row, saved = rows[count], data[count]
            keys = list(dict.fromkeys((key, *original["source_ids"])))
            if (
                row.owner_id != owner
                or row.purpose != "transport.borrowed-headroom"
                or saved["source_ids"] != keys
                or not row.geometry.equals_exact(shape, 0)
                or row.origin_xy_m != physical.origin_xy_m
                or row.lower_plane_m != physical.upper_plane_m
                or row.upper_plane_m[1:] != physical.upper_plane_m[1:]
            ):
                raise ValueError(
                    "borrowed source/physical owner/mask/height seam changed"
                )
            top = asset.data["body_vertices"][asset.data["top_faces"][face]]
            residuals = [
                plane_value(row.upper_plane_m, row.origin_xy_m, p[:2])
                - Fraction(float(p[2]))
                - Fraction(required)
                for p in top
            ]
            if min(residuals) < 0 or max(residuals) > Fraction(1, 100000000):
                raise ValueError("borrowed headroom fails whole original facet heights")
            maximum = max(maximum, *residuals)
            actual_records.append(
                {
                    "owner_id": owner,
                    "physical_owner_id": identity,
                    "logical_source_id": key,
                    "physical_source_ids": original["source_ids"],
                    "base_clearance_m": old_height,
                    "required_clearance_m": required,
                    "asset_sha256": asset.identity,
                    "asset_top_face_index": int(face),
                }
            )
            count += 1
    if count != len(rows) or actual_records != report.get("borrowed_headroom", {}).get(
        "records", []
    ):
        raise ValueError("complete borrowed headroom provenance ledger differs")
    return count - first, float(maximum)
