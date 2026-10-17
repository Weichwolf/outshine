"""Whole varying source profiles, using explicit reuse of the immutable old cap."""

import numpy as np
from stair_cycle_contacts import interior_profile_xy, profile
from stair_module_trace import compare


def owned(data, owner, old_caps, borrowed):
    caps = [
        (
            data["surface_vertices"],
            data["surface_faces"][data["owners"][:, owner]],
            None,
        )
    ]
    if borrowed is not None and not borrowed.is_empty:
        caps.extend(
            (old_cap.vertices, old_cap.triangles, borrowed) for old_cap in old_caps
        )
    return caps


def contact_profiles(
    rows, nodes, root, frozen_cap, frozen_line, contacts, root_interior=False
):
    old_at = {root: [frozen_cap]}
    references = [(root, frozen_cap, frozen_line[:, :2], root_interior)]
    keys = [r["source_id"] for r in rows]
    for contact in contacts:
        choices = [
            i for i, node in enumerate(nodes) if node["mvt_xy"] == contact["mvt_xy"]
        ]
        if len(choices) != 1:
            raise ValueError("declared original interior node missing from saved graph")
        index = choices[0]
        if nodes[index]["incident"] != sorted(
            keys.index(k) for k in contact["incident_source_ids"]
        ):
            raise ValueError("cached interior contact source membership changed")
        old_at.setdefault(index, []).append(contact["cap"])
        references.append((index, contact["cap"], contact["profile"][:, :2], True))
    return old_at, references


def frozen_profile_checks(data, nodes, masks, original, old_at, references):
    checks = []
    for index, cap, line, interior in references:
        original_cap = [(cap.vertices, cap.triangles, masks[index])]
        new_owners = []
        for owner in nodes[index]["incident"]:
            new_owners.extend(
                owned(
                    data,
                    owner,
                    old_at[index],
                    original[owner].intersection(masks[index]),
                )
            )
        checks.append(
            {
                "cached_interior": interior,
                "checks": [
                    compare([original_cap, new_owners], line, cast)
                    for cast in (False, True)
                ],
            }
        )
    return checks


def new_interior_checks(data, rows, nodes, masks, original, old_at):
    result = []
    for index, node in enumerate(nodes):
        for owner in node["incident"]:
            row = rows[owner]
            found = np.flatnonzero(np.all(row["xy"] == node["xy"], axis=1))
            if len(found) != 1 or not 0 < found[0] < len(row["xy"]) - 1:
                continue
            own = owned(
                data,
                owner,
                old_at.get(index, []),
                original[owner].intersection(masks[index]) if index in old_at else None,
            )
            other = []
            for neighbour in node["incident"]:
                if neighbour != owner:
                    other.extend(
                        owned(
                            data,
                            neighbour,
                            old_at.get(index, []),
                            original[neighbour].intersection(masks[index])
                            if index in old_at
                            else None,
                        )
                    )
            other.extend(
                (cap.vertices, cap.triangles, masks[index])
                for cap in old_at.get(index, [])
            )
            line = interior_profile_xy(row, int(found[0]))
            checks = [compare([own, other], line, cast) for cast in (False, True)]
            result.append(
                {
                    "source_id": row["source_id"],
                    "vertex": int(found[0]),
                    "checks": checks,
                }
            )
    return result


def varying_profiles(
    data,
    rows,
    nodes,
    masks,
    original,
    frozen_cap,
    frozen_line,
    *,
    contacts=(),
    root_interior=False,
):
    root = next(
        i
        for i, node in enumerate(nodes)
        if np.array_equal(node["xy"], frozen_line[1, :2])
    )
    old_at, references = contact_profiles(
        rows, nodes, root, frozen_cap, frozen_line, contacts, root_interior
    )
    result, outer = [], []
    for owner, row in enumerate(rows):
        for end in (0, -1):
            choices = [
                i
                for i, node in enumerate(nodes)
                if owner in node["incident"]
                and np.array_equal(node["xy"], row["xy"][end])
            ]
            if not choices:
                outer.append({"source_id": row["source_id"], "end": end})
                continue
            index = choices[0]
            borrow = (
                original[owner].intersection(masks[index]) if index in old_at else None
            )
            own = owned(data, owner, old_at.get(index, []), borrow)
            others = []
            for neighbour in nodes[index]["incident"]:
                if neighbour != owner:
                    region = (
                        original[neighbour].intersection(masks[index])
                        if index in old_at
                        else None
                    )
                    others.extend(owned(data, neighbour, old_at.get(index, []), region))
            others.extend(
                (cap.vertices, cap.triangles, masks[index])
                for cap in old_at.get(index, [])
            )
            checks = [
                compare([own, others], profile(row, end), cast)
                for cast in (False, True)
            ]
            result.append({"source_id": row["source_id"], "end": end, "checks": checks})
    old_checks = frozen_profile_checks(data, nodes, masks, original, old_at, references)
    return {
        "new_internal_profiles": result,
        "old_profile_completed": not root_interior,
        "old_profile_checks": old_checks,
        "complete_cached_interior_profiles": len(contacts) + int(root_interior),
        "outer_profiles": outer,
        "complete_additional_profiles": len(result) + int(not root_interior),
        **(
            {
                "new_source_interior_profiles": new_interior_checks(
                    data, rows, nodes, masks, original, old_at
                )
            }
            if root_interior
            else {}
        ),
    }
