"""Chosen bridge-over-bridge order without changing retained bridge source kinds."""


def apply_orders(rows, declarations):
    if not isinstance(declarations, list) or len(declarations) > 8:
        raise ValueError("at most eight explicit bridge separation orders required")
    if any(r.get("separation_below_source_ids") for r in rows):
        raise ValueError("bridge orders must start from unmarked original sources")
    keys = [r["source_id"] for r in rows]
    chosen, seen = [], set()
    for item in declarations:
        lower, upper = item["lower"], item["upper"]
        pair = frozenset((lower, upper))
        if len(pair) != 2 or pair in seen or not pair <= set(keys):
            raise ValueError("unique distinct original bridge pair required")
        low, high = keys.index(lower), keys.index(upper)
        if not all(
            rows[i]["original"]["bridge"]
            and rows[i]["original"]["source"]["feature"]["properties"].get("brunnel")
            == "bridge"
            for i in (low, high)
        ):
            raise ValueError("explicit separation requires two actual bridge sources")
        chosen.append((low, high))
        seen.add(pair)
    notes = []
    for low, high in chosen:
        rows[high].setdefault("separation_below_source_ids", []).append(keys[low])
        notes.append(
            {
                "lower": keys[low],
                "upper": keys[high],
                "lower_layer_hint": rows[low]["original"]["source"]["feature"][
                    "properties"
                ].get("layer"),
                "upper_layer_hint": rows[high]["original"]["source"]["feature"][
                    "properties"
                ].get("layer"),
                "scope": "Chosen whole bridge-over-bridge recipe; source kinds retained, layer is a hint, fitted heights are not measurements.",
            }
        )
    return notes


def crossing_order(rows, i, j):
    a, b = rows[i], rows[j]
    a_above = b["source_id"] in a.get("separation_below_source_ids", [])
    b_above = a["source_id"] in b.get("separation_below_source_ids", [])
    if a_above and b_above:
        raise ValueError("contradictory chosen bridge separation order")
    if a_above:
        return j, i
    if b_above:
        return i, j
    if a["original"]["bridge"] == b["original"]["bridge"]:
        return None
    return (j, i) if a["original"]["bridge"] else (i, j)
