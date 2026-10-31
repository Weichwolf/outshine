"""Bind original render-part/face IDs to captured primitive WORD ranges and sources."""

import gzip
import json


def bind(root, part_face_pairs, plan_key):
    def read(name):
        return json.loads(gzip.decompress((root / name).read_bytes()))

    ranges = read("road-primitive-ranges.json.gz")
    bindings = read("road-bindings.json.gz")
    roads = read("roads.json.gz")
    if any(d["plan_key"] != plan_key for d in (ranges, bindings, roads)):
        raise ValueError("same original road plan required")
    pages = {p["render_part"]: p for p in ranges["pages"]}
    plan = {tuple(p["part"]): p["lane_contacts"] for p in bindings["plan_bindings"]}
    ways = {w["ordinal"]: w for w in roads["source_ways"]}
    result = []
    for part, face in part_face_pairs:
        page = pages.get(int(part))
        rows = (
            []
            if page is None
            else [
                row
                for row in page["ranges"]
                if row["index_word_range"][0] <= int(face) * 3
                and int(face) * 3 + 3 <= sum(row["index_word_range"])
            ]
        )
        if len(rows) != 1:
            raise ValueError(
                "one captured original primitive range per road face required"
            )
        row = rows[0]
        refs = plan[tuple(row["part"])]
        result.append(
            {
                "render_part": int(part),
                "face": int(face),
                "page_key": page["page_key"],
                "primitive": row,
                "lane_contacts": refs,
                "source_ways": [ways.get(lane) for lane, _ in refs],
            }
        )
    return result


def interfaces(root, nodes, plan_key):
    pairs = sorted({tuple(pair) for values in nodes for pair in values})
    indices = {pair: i for i, pair in enumerate(pairs)}
    records = bind(root, pairs, plan_key)
    ways = {}
    for row in records:
        sources = row.pop("source_ways")
        row["source_way_ordinals"] = [
            None if w is None else w["ordinal"] for w in sources
        ]
        ways.update((w["ordinal"], w) for w in sources if w is not None)
    return {
        "plan_key": plan_key,
        "node_faces": [[indices[tuple(pair)] for pair in values] for values in nodes],
        "faces": records,
        "source_ways": list(ways.values()),
    }
