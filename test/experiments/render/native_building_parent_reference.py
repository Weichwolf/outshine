"""Separate native shading agreement from first-hit raster boundary differences."""

import json

import numpy as np
from building_mass_lighting import shade
from building_mass_material_image import evaluate, samples
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_transfer import transfers
from compare_building_brdf import fitted_cut
from compare_building_hierarchy import complete
from compare_mass_appearance import read
from native_building_cell_reference import load_cells
from triangle_image_reference import image_rays


def native_face_response(original, fields, full, camera, box, gpu, audit, tables):
    height, width = gpu.shape[:2]
    ids = audit[:, :, 0].astype(int).ravel() - 1
    hit = ids >= 0
    if np.any(ids[hit] >= len(original["indices"])):
        raise ValueError("native primitive audit exceeds original face identities")
    state = audit[:, :, 3].astype(int).ravel()[hit]
    if not np.isin(state, [0, 1]).all():
        raise ValueError("native primitive audit has an invalid material state")
    _, rays = image_rays(camera, box, width, height, 1)
    outgoing = -rays[hit] / np.linalg.norm(rays[hit], axis=1)[:, None]
    faces = ids[hit]
    response, _ = shade(
        fields["albedo"][faces, state],
        fields["weights"][faces, state],
        full["roughness_palette"],
        fields["normal"][faces],
        outgoing,
        (0.45, -0.5, 0.7),
        tables,
    )
    reference = samples(original, camera, box, 1)
    reference_ids = np.full(width * height, -1)
    reference_ids[reference["hit"]] = reference["faces"]
    changed = np.flatnonzero(ids != reference_ids)
    return {
        "native_hit_pixels": int(hit.sum()),
        "reference_hit_pixels": int(reference["hit"].sum()),
        "actual_native_face_GGX_max_abs": float(
            np.max(np.abs(gpu[:, :, :3].reshape(-1, 3)[hit] - response), initial=0)
        ),
        "different_first_faces": [
            {
                "pixel": [int(at // width), int(at % width)],
                "native_face": int(ids[at]),
                "reference_face": int(reference_ids[at]),
            }
            for at in changed
        ],
    }


def compare(output, parents, fits, archive, controls, tables):
    report = json.loads((parents / "report.json").read_text())
    tree = read_tree(parents)
    original, _ = load_cells(controls, archive, set(report["cells"]), tree)
    fields, unsupported = face_materials(original, policy())
    if unsupported:
        raise ValueError("original material controls remain unsupported")
    tables = read(tables)
    results = {}
    for query in report["queries"]:
        folder = output / query["name"]
        meta = json.loads((folder / "input.json").read_text())
        camera, box = view(query), np.array(meta["box"])
        product, full, _ = load_cut(parents, report, tree, query, policy())
        two, _ = fitted_cut(parents, fits, report, query, full, 2)
        mesh, _ = complete(original, product)
        expected = {
            "original": evaluate(
                original,
                original,
                fields,
                full,
                product,
                camera,
                box,
                tables,
                supersample=1,
            )[0],
            "opaque-full": evaluate(
                mesh,
                original,
                fields,
                full,
                product,
                camera,
                box,
                tables,
                parent=True,
                supersample=1,
            )[0],
        }
        for label, appearance in (("fractional-full", full), ("fractional-two", two)):
            expected[label] = transfers(
                mesh, original, fields, product, appearance, camera, box, tables, 1
            )[0]
        local = {}
        for label, reference in expected.items():
            gpu = np.fromfile(folder / (label + ".rgba32f"), "<f4").reshape(
                meta["height"], meta["width"], 4
            )
            if not np.isfinite(gpu).all():
                raise ValueError("native surface output is not finite")
            delta = np.abs(gpu - np.dstack((reference["C"], reference["T"])))
            local[label] = {
                "C_max_abs": float(delta[:, :, :3].max()),
                "T_max_abs": float(delta[:, :, 3].max()),
                "scope": "Point-sampled native Float32 raster versus Double reference; not an image gate.",
            }
            if label == "original" and (folder / "original.audit32f").exists():
                audit = np.fromfile(folder / "original.audit32f", "<f4").reshape(
                    gpu.shape
                )
                local[label]["audit"] = native_face_response(
                    original, fields, full, camera, box, gpu, audit, tables
                )
        results[query["name"]] = local
    return results
