"""Whole-population insertion proof and actual lower-service comparison inputs."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from cooling_service_mesh import components, world_points
from cooling_service_plan import plan
from cooling_tower_details import refine
from cooling_tower_plan import plan as tower_plan
from cooling_tower_probe import isolated, sources
from demand import View
from evidence import revision
from facade_parts import append
from industrial_relief_probe import export
from industrial_relief_products import ArticulatedDetails
from prepared_hall_tile import prepare_halls
from recipe_graph_data import plain


def extra_mesh(access):
    chunks = [
        dict(m, facade_extent=np.zeros((len(m["vertices"]), 3)))
        for m in components(access).values()
    ]
    return append(chunks[0], chunks[1:])


def exact_insertion(before, after, accesses):
    remove = np.zeros(len(after["vertices"]), bool)
    faces_removed = np.zeros(len(after["faces"]), bool)
    entries = []
    for access in accesses:
        extra = extra_mesh(access)
        size = len(extra["vertices"])
        candidates = np.flatnonzero(
            np.all(after["vertices"] == extra["vertices"][0], axis=1)
        )
        matches = []
        for start in candidates:
            if all(
                after[k][start : start + size].dtype == v.dtype
                and after[k][start : start + size].tobytes() == v.tobytes()
                for k, v in extra.items()
                if k not in ("faces", "materials")
            ):
                matches.append(int(start))
        if len(matches) != 1:
            raise ValueError(
                "service arrays must occur exactly once in actual whole product"
            )
        start = matches[0]
        if remove[start : start + size].any():
            raise ValueError("service insertions overlap")
        remove[start : start + size] = True
        selected = ((after["faces"] >= start) & (after["faces"] < start + size)).all(
            axis=1
        )
        if not np.array_equal(after["faces"][selected] - start, extra["faces"]):
            raise ValueError("whole product changed service triangle topology")
        if (
            after["materials"][selected].dtype != extra["materials"].dtype
            or after["materials"][selected].tobytes() != extra["materials"].tobytes()
        ):
            raise ValueError("whole product changed service face material words")
        faces_removed |= selected
        entries.append(
            {
                "source_id": access.tower.source.identifier,
                "vertices": size,
                "triangles": int(selected.sum()),
            }
        )
    if not np.array_equal(remove[after["faces"]].any(axis=1), faces_removed):
        raise ValueError("service insertion connected to an unrelated source triangle")
    for key in before.keys() - {"faces"}:
        kept = after[key][~faces_removed if key == "materials" else ~remove]
        if (
            kept.dtype != before[key].dtype
            or kept.shape != before[key].shape
            or kept.tobytes() != before[key].tobytes()
        ):
            raise ValueError("service insertion changed previous product field: " + key)
    remap = np.cumsum(~remove) - 1
    if not np.array_equal(remap[after["faces"][~faces_removed]], before["faces"]):
        raise ValueError("service insertion changed previous source topology")
    return entries


def neighbour_check(rows, accesses):
    results = []
    for access in accesses:
        space = list(components(access).values()) + list(
            components(access, clearance=True).values()
        )
        polygon = shapely.MultiPoint(
            np.vstack([m["vertices"][:, :2] for m in space])
        ).convex_hull
        own = access.tower.source.identifier
        overlaps = [
            r["id"]
            for r in rows
            if r["layer"] == "building"
            and r["id"] != own
            and polygon.intersects(r["polygon"])
        ]
        if overlaps:
            raise ValueError(
                "service space intersects a delivered neighbour footprint: "
                + str(overlaps)
            )
        results.append(
            {
                "source_id": own,
                "other_building_footprints_checked": sum(
                    r["layer"] == "building" and r["id"] != own for r in rows
                ),
                "overlaps": overlaps,
            }
        )
    return results


def pair(output, label, rows, receipt, view, proof, error_m):
    prepared = prepare_halls(rows)
    records, meshes, accesses = [], [], []
    for enabled, suffix in ((False, "Before"), (True, "After")):

        def radial(row, source, info, error, enabled=enabled):
            value = refine(row, source, info, error, service_access=enabled)
            if enabled and value is not None and "service_access" in value[1]:
                accesses.append(plan(tower_plan(source)))
            return value

        owner = ArticulatedDetails(prepared, view)
        name = label + suffix
        result = export(
            output,
            name,
            owner,
            prepared.rows,
            receipt,
            view,
            proof,
            radial_builder=radial,
            error_m=error_m,
        )
        path = output / (name + "-geometry.json")
        record = json.loads(path.read_text())
        record["scope"] = (
            "Actual retained provider population; optional bounded lower cooling-tower ladder/landing insertion. Source geometry/IDs/heights and previous products retained. Flat diagnostic ground; no full tower access, controller, structural or native device admission."
        )
        path.write_text(json.dumps(record, default=plain, indent=2) + "\n")
        with np.load(record["mesh"], allow_pickle=False) as archive:
            meshes.append(dict(archive))
        records.append(record)
        if result["generated"] != sum(r["layer"] == "building" for r in rows):
            raise ValueError("service comparison omitted a retained source")
    facts = (
        "id",
        "source_properties",
        "footprint_wkb",
        "kind",
        "origin",
        "clipped",
        "source_height_interval",
        "colour_origin",
    )
    for a, b in zip(
        records[0]["source"]["members"], records[1]["source"]["members"], strict=True
    ):
        if any(a[k] != b[k] for k in facts):
            raise ValueError("service comparison changed retained source facts")
    if (
        records[0]["source"]["material_definitions"]
        != records[1]["source"]["material_definitions"]
    ):
        raise ValueError("service insertion changed material bindings")
    inserted = exact_insertion(*meshes, accesses)
    return {
        "sources": len(records[0]["source"]["members"]),
        "before_triangles": len(meshes[0]["faces"]),
        "after_triangles": len(meshes[1]["faces"]),
        "previous_fields_and_topology_word_exact": True,
        "insertions": inserted,
        "neighbours": neighbour_check(rows, accesses),
    }


def sections(accesses, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle

    figure, axes = plt.subplots(1, len(accesses), figsize=(12, 7), squeeze=False)
    for ax, access in zip(axes[0], accesses, strict=True):
        for profile in (
            access.tower.footing,
            access.tower.collector,
            access.tower.shell,
        ):
            values = np.array(profile.knots)
            ax.fill(
                np.r_[values[:, 1], values[::-1, 2]],
                np.r_[values[:, 0], values[::-1, 0]],
                color="lightgray",
                edgecolor="gray",
            )
        for boxes, color in ((access.parts, "black"), (access.clearances, "green")):
            for part in boxes:
                c, half = np.array(part.centre), np.array(part.extent) / 2
                if abs(c[1]) <= half[1]:
                    ax.add_patch(
                        Rectangle(
                            (c[0] - half[0], c[2] - half[2]),
                            2 * half[0],
                            2 * half[2],
                            facecolor=color,
                            alpha=0.25 if color == "green" else 0.8,
                        )
                    )
        route = np.array(access.route)
        ax.plot(route[:, 0], route[:, 2], "b.-", label="foot route")
        radius = access.tower.source.knots[0][1]
        ax.set(
            xlim=(radius - 3.4, radius + 0.4),
            ylim=(access.ground_m - 0.3, access.floor_m + 2.4),
            xlabel="local radial x / m",
            ylabel="height / m",
            title=f"{len(access.parts)} connected service parts; y=0 slice",
        )
        ax.set_aspect("equal")
        ax.legend()
    figure.tight_layout()
    figure.savefig(output / "service-sections.png", dpi=150)
    plt.close(figure)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "freeze service construction before actual population evidence"
        )
    output.mkdir(parents=True, exist_ok=False)
    rows, receipt, view, towers = sources()
    accesses = [plan(tower) for _row, tower in towers]
    sections(accesses, output)
    reports = {"whole": pair(output, "ServiceWhole", rows, receipt, view, proof, 1.0)}
    for index, ((row, tower), access) in enumerate(zip(towers, accesses, strict=True)):
        radius = tower.source.knots[0][1]
        close = View(
            world_points(access, [[radius + 12, -3, access.floor_m + 3]])[0],
            world_points(access, [[radius - 1.3, 0, access.floor_m / 2]])[0],
            width=1920,
            height=1080,
            fov_deg=48,
        )
        reports[f"close-{index}"] = pair(
            output,
            f"ServiceClose{index}",
            isolated(rows, row),
            receipt,
            close,
            proof,
            0.05,
        )
    report = proof | {
        "comparisons": reports,
        "scope": "Bounded Python whole-population insertion/functional diagnostic, no World/device admission",
    }
    (output / "report.json").write_text(
        json.dumps(report, default=plain, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                k: {
                    p: v[p]
                    for p in (
                        "sources",
                        "before_triangles",
                        "after_triangles",
                        "previous_fields_and_topology_word_exact",
                    )
                }
                for k, v in reports.items()
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    models(parser.parse_args().output)
