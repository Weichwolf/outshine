"""Transport-owned source semantics, separate from the material owner's recipes."""

import shapely

FAMILIES = (
    "unassigned",
    "carriageway_paved",
    "carriageway_default",
    "embedded_rail_default",
    "unpaved",
    "path_paved",
    "path_default",
    "rail_bed_default",
    "structure_default",
)
MATERIALS = (
    None,
    "asphalt",
    "asphalt",
    "asphalt",
    "earth_dry",
    "paving",
    "paving",
    "crushed_stone",
    "paving",
)
MOTOR = {
    "motorway",
    "trunk",
    "primary",
    "secondary",
    "tertiary",
    "minor",
    "service",
    "track",
    "busway",
}


def source_family(properties):
    kind, surface = properties.get("class"), properties.get("surface")
    if kind == "rail":
        return 7, "procedural_rail_bed"
    if kind == "transit":
        return (
            (3, "procedural_embedded_rail")
            if properties.get("subclass") == "tram"
            else (0, "unplanned_transit_surface")
        )
    if kind == "pier":
        return 8, "procedural_structure_surface"
    if kind not in MOTOR | {"path"}:
        return 0, "unplanned_source_class"
    if surface == "unpaved":
        return 4, "tag_unpaved_recipe_earth_dry"
    if surface not in (None, "paved"):
        return 0, "unplanned_surface_tag"
    if kind == "path":
        return (
            (5, "tag_paved_recipe_paving")
            if surface
            else (6, "procedural_path_surface")
        )
    return (
        (1, "tag_paved_recipe_asphalt")
        if surface
        else (2, "procedural_carriageway_surface")
    )


def family_domains(records, *, max_records=8192):
    if not 1 <= len(records) <= max_records:
        raise ValueError("bounded actual source corridors required")
    buckets = [[] for _ in FAMILIES]
    for record in records:
        if record["family"]:
            buckets[record["family"]].append(record["shape"])
    domains, report = [], []
    occupied = shapely.Polygon()
    for family, bucket in enumerate(buckets):
        raw = shapely.union_all(bucket)
        domain = raw.difference(occupied) if family else shapely.Polygon()
        if not domain.is_valid:
            raise ValueError("valid finite family domain required")
        domains.append(domain)
        occupied = occupied.union(domain)
        report.append(
            {
                "family": FAMILIES[family],
                "source_corridors": len(bucket),
                "raw_area_m2": raw.area,
                "area_m2": domain.area,
                "precedence_removed_m2": max(0.0, raw.area - domain.area),
            }
        )
    return domains, report


def graph_records(graphs, raw, valid_ids):
    records = []
    for graph_id, graph in enumerate(graphs):
        bands = shapely.buffer(
            graph["lines"],
            graph["half_widths"],
            quad_segs=2,
            cap_style="round",
            join_style="round",
        )
        for edge, (ids, shape) in enumerate(zip(graph["sources"], bands, strict=True)):
            used = [s for s in ids if s in valid_ids]
            families = {source_family(raw[s]["properties"])[0] for s in used}
            family = next(iter(families)) if len(families) == 1 else 0
            records.append(
                {
                    "graph": graph_id,
                    "edge": edge,
                    "sources": list(ids),
                    "used_sources": used,
                    "half_width_m": float(graph["half_widths"][edge]),
                    "family": family,
                    "shape": shape,
                    "basis": "source_bound"
                    if len(families) == 1
                    else "unused_or_ambiguous_edge_material",
                    "source_basis": [
                        source_family(raw[s]["properties"])[1] for s in used
                    ],
                }
            )
    return records


def old_ground_records(sources):
    records = []
    for source in sources:
        if source["role"] != "ground":
            continue
        family, basis = source_family(source["properties"])
        shape = shapely.LineString(source["xy"]).buffer(
            source["width_m"] / 2, cap_style="flat", join_style="mitre", mitre_limit=2
        )
        records.append(
            {
                "sources": [source["source_id"]],
                "used_sources": [source["source_id"]],
                "family": family,
                "basis": basis,
                "shape": shape,
                "half_width_m": source["width_m"] / 2,
                "old_role": "ground",
            }
        )
    return records
