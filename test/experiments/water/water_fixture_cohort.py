"""Shared finite WATER3 fixture selection for cell and interface experiments."""

from water_body_domains import assemble
from water_cell_edges import align_edges, edge_knots
from water_cell_handoff import route_cells
from water_grid_height import prepare_grid


def prepare_cohort(plan, snapshots, data, course_order, *, cells):
    if plan.state != "ready" or not plan.version:
        raise ValueError(f"no owner before required source layers arrive: {plan.state}")
    products, handoff = route_cells(
        snapshots,
        source_tiles=plan.tiles,
        recipients=plan.tiles,
        scope="fixed-inputs:" + plan.version,
    )
    knots = edge_knots(products)
    products, edge_work = align_edges(products, knots)
    records = [
        (tile, p, c.parents[tile + (p.feature, p.part)], c.snapshot.source)
        for tile, c in products.items()
        for p in c.snapshot.pieces
    ]
    bodies, rejected, _ = assemble(
        [r[1].polygon for r in records],
        [r[1].info for r in records],
        [r[3] for r in records],
        assume_ground=True,
    )
    if rejected:
        raise ValueError("fixed finite source body rejected")
    candidates = [
        b
        for b in bodies
        if records[b.sources[0]][1].info["kind"] == "river"
        and all(any(records[i][0] == cell for i in b.sources) for cell in cells)
    ]
    if not candidates:
        raise ValueError("fixture cells have no shared admitted river component")
    body = min(
        candidates,
        key=lambda b: (
            -len(b.sources),
            tuple(sorted(records[i][2] for i in b.sources)),
        ),
    )
    records = [records[i] for i in body.sources]
    original_courses = [i for tile in plan.tiles for i in course_order[tile]]
    scoped = dict(data, courses=data["courses"][original_courses])
    prepared = prepare_grid(records, scoped, 1024)
    return (
        prepared,
        records,
        knots,
        {
            "handoff": handoff,
            "edge_work": edge_work,
            "selected_original_course_table_indices": [
                original_courses[i]
                for i in prepared.report["sources"]["selected_original_courses"]
            ],
            "finite_course_table_original_indices": original_courses,
            "scoped_course_table_copied_bytes": scoped["courses"].nbytes,
            "body_original_keys": sorted({r[2] for r in records}),
            "prepared": prepared.report,
        },
    )
