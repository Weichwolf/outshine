"""Adapt existing checked source-parent products to a bounded native cost probe."""

import json
from pathlib import Path

import numpy as np
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from compare_building_brdf import fitted_cut
from compare_building_hierarchy import complete
from compare_building_regional_hierarchy import rest_price
from compare_directed_groups import retained_subset
from fit_building_brdf import validate_fit_receipt
from native_building_cell_reference import load_cells
from triangle_image_reference import visible_pixel_bounds


def original_records(original, fields, faces):
    count = len(faces)
    records = np.zeros((count, 11, 4), np.float32)
    corners = original["indices"][faces, 0]
    records[:, 0, :3] = original["positions"][corners]
    records[:, 0, 3] = original["uv"][corners, 1]
    records[:, 1, :3] = fields["uv_gradient"][faces]
    records[:, 1, 3] = 1
    records[:, 2, :3] = fields["normal"][faces]
    records[:, 2, 3] = 1
    records[:, 3:5, :3] = fields["albedo"][faces]
    for state in (0, 1):
        records[:, 5 + state * 3 : 8 + state * 3].reshape(count, 12)[:, :9] = fields[
            "weights"
        ][faces, state]
    return records


def parent_records(product, appearance):
    triangles = product["positions"].astype(float)[product["indices"]]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    length = np.linalg.norm(normal, axis=1)
    if np.any(length == 0):
        raise ValueError("native probe requires nondegenerate parent triangles")
    normal /= length[:, None]
    groups = product["triangle_groups"]
    role = (normal[:, 2] > 0.5).astype(int)
    records = np.zeros((len(normal), 11, 4), np.float32)
    records[:, 2, :3] = normal
    records[:, 2, 3] = np.sum(
        np.abs(normal) * product["group_coverage"][groups][:, [2, 1, 0]], axis=1
    ) / np.abs(normal).sum(axis=1)
    records[:, 3, :3] = records[:, 4, :3] = appearance["albedo"][groups, role]
    for state in (0, 1):
        records[:, 5 + state * 3 : 8 + state * 3].reshape(len(normal), 12)[:, :9] = (
            appearance["roughness_weights"][groups, role]
        )
    return records


def frame(query, positions, palette):
    camera = view(query)
    box = visible_pixel_bounds(positions, camera)
    width, height = box[1] - box[0]
    eye, basis, focal, screen_width, screen_height = camera
    right, up, forward = basis
    clip = np.zeros((4, 4))
    clip[0, :3] = (
        2 * focal / width * right
        + (screen_width - 2 * box[0, 0] - width) / width * forward
    )
    clip[1, :3] = (
        2 * focal / height * up
        - (screen_height - 2 * box[0, 1] - height) / height * forward
    )
    clip[3, :3] = forward
    clip[:, 3] = -clip[:, :3] @ eye
    clip[2, :3], clip[2, 3] = 0, 0.1
    if len(palette) != 9 or min(width, height) <= 0:
        raise ValueError(
            "the fixed native probe requires nine palette entries and a crop"
        )
    padded = np.pad(palette, (0, 3))
    return box, np.concatenate((clip.T.ravel(), np.r_[eye, 0], padded)).astype("<f4")


def write_case(root, positions, indices, records, layers, opaque):
    root.mkdir()
    for name, values, dtype in (
        ("positions", positions, "<f4"),
        ("indices", indices, "<u4"),
        ("records", records, "<f4"),
    ):
        np.asarray(values, dtype=dtype).tofile(root / (name + ".bin"))
    return {
        "name": root.name,
        "layers": layers,
        "opaque": opaque,
        "triangles": len(indices),
        "bytes": sum(path.stat().st_size for path in root.iterdir()),
    }


def prepare_query(root, query, original, fields, product, full, two):
    root.mkdir()
    box, uniform = frame(
        query,
        np.vstack((original["positions"], product["positions"])),
        full["roughness_palette"],
    )
    uniform.tofile(root / "frame.bin")
    faces = np.arange(len(original["indices"]))
    cases = [
        write_case(
            root / "original",
            original["positions"],
            original["indices"],
            original_records(original, fields, faces),
            [[0, 3 * len(faces)]],
            True,
        )
    ]
    rest = np.flatnonzero(np.isin(original["binding"][:, 1], product["needs_sources"]))
    retained = retained_subset(original, product["needs_sources"])
    positions = np.vstack((product["positions"], retained["positions"]))
    rest_indices = retained["indices"] + len(product["positions"])
    groups = [
        np.flatnonzero(product["triangle_groups"] == group)
        for group in range(len(full["albedo"]))
    ]
    groups = [group for group in groups if len(group)]
    if len(groups) + bool(len(rest)) > 16:
        raise ValueError("the bounded component probe supports at most sixteen layers")
    layers, first = [], 0
    for count in [len(rest), *map(len, groups)]:
        if count:
            layers.append([first, count * 3])
        first += count * 3
    indices = np.vstack(
        (rest_indices, *[product["indices"][group] for group in groups])
    )
    for kind, material in (("full", full), ("two", two)):
        records = parent_records(product, material)
        records = np.vstack(
            (
                original_records(original, fields, rest),
                *[records[group] for group in groups],
            )
        )
        cases.append(
            write_case(
                root / ("fractional-" + kind),
                positions,
                indices,
                records,
                layers,
                False,
            )
        )
        if kind == "full":
            cases.append(
                write_case(
                    root / "opaque-full",
                    positions,
                    indices,
                    records,
                    [[0, len(indices) * 3]],
                    True,
                )
            )
    width, height = box[1] - box[0]
    result = {
        "width": int(width),
        "height": int(height),
        "box": box.tolist(),
        "query": query,
        "cases": cases,
    }
    (root / "input.json").write_text(json.dumps(result, indent=2))
    return result


def prepare(parents, fits, archive, controls, output):
    report = json.loads((parents / "report.json").read_text())
    tree = read_tree(parents)
    tables = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    validate_fit_receipt(fits, 2, tables)
    original, provenance = load_cells(controls, archive, set(report["cells"]), tree)
    at = policy()
    fields, unsupported = face_materials(original, at)
    if unsupported:
        raise ValueError("original material controls remain unsupported")
    rows = []
    for query in report["queries"]:
        product, full, loading = load_cut(parents, report, tree, query, at)
        two, fitted_bytes = fitted_cut(parents, fits, report, query, full, 2)
        meta = prepare_query(
            output / query["name"], query, original, fields, product, full, two
        )
        _, complete_price = complete(original, product)
        rows.append(
            {
                "query": query["name"],
                "loading": loading,
                "fitted_bytes": fitted_bytes,
                "geometry": complete_price,
                "original_rest": rest_price(
                    original, product, provenance["unresolved_originals"], fields=fields
                ),
                "raster": meta,
            }
        )
    return {"provenance": provenance, "queries": rows}
