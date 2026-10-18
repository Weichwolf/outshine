"""Camera-only roof-plan comparison; full native/world acceptance remains separate."""

import argparse
import hashlib
import json
import tempfile
import time
from collections import Counter
from pathlib import Path

import numpy as np
from building_roof_domains import planar_roof_domains, triangulate_domain
from building_roof_positions import roof_positions
from current_visibility import camera, project, raster
from outshine_ref.geometry.occlusion import convex_patch_umbra
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy
from PIL import Image


def emit_roof(source, *, vertex_recipe="float64"):
    """Emit only an admitted source's roofs; unknown forms stay explicit."""
    surface, parts, omissions = source["surface"], [], Counter()
    if surface is None:
        return np.empty((0, 3, 3)), Counter({"unknown_source_surface": 1})
    for shape in surface["shapes"]:
        try:
            pieces = planar_roof_domains(shape)
            triangles = [triangulate_domain(piece) for piece in pieces]
        except ValueError as error:
            omissions[str(error)] += 1
            continue
        if not pieces:
            omissions["no_original_planar_domains"] += 1
        for piece, triangle in zip(pieces, triangles, strict=True):
            parts.append(
                roof_positions(
                    triangle, piece["plane_enu"], surface, recipe=vertex_recipe
                )
            )
    return (
        np.concatenate(parts) if parts else np.empty((0, 3, 3)),
        omissions,
    )


def seeds(sources, bounds, known, eye, observer, *, vertex_recipe="float64"):
    """Bounded first emission; accepted source roofs are retained in every plan."""
    centre = bounds.mean(axis=1)
    distance = np.linalg.norm(centre - eye, axis=1)
    order = np.flatnonzero(known)
    order = order[np.argsort(distance[order], kind="stable")][:32]
    emitted, occluders, counts = {}, [], Counter()
    for index in order:
        triangles, omissions = emit_roof(sources[index], vertex_recipe=vertex_recipe)
        emitted[int(index)] = (triangles, omissions)
        for triangle in triangles:
            counts["seed_triangles_examined"] += 1
            if (triangle < bounds[index, 0]).any() or (
                triangle > bounds[index, 1]
            ).any():
                counts["seed_triangles_outside_native_bound"] += 1
                continue
            try:
                umbra = convex_patch_umbra(triangle, observer, certified_opaque=True)
            except ValueError as error:
                counts[f"uncertifiable_seed_triangle: {error}"] += 1
                continue
            if len(umbra.low):
                occluders.append((umbra, int(index)))
            if len(occluders) >= 256:
                break
        if len(occluders) >= 256:
            break
    protected = np.zeros(len(sources), dtype=bool)
    protected[list(emitted)] = True
    counts.update(
        retained_seed_sources=len(emitted),
        accepted_seed_roof_triangles=sum(len(item[0]) for item in emitted.values()),
        occluder_triangles=len(occluders),
    )
    return emitted, occluders, protected, counts


def selections(sources, bounds, known, occluders, protected):
    start = time.perf_counter()
    global_tree = BoundsHierarchy(bounds, known=known)
    build_s = time.perf_counter() - start
    start = time.perf_counter()
    joint = global_tree.select(occluders, protected=protected)
    joint_s = time.perf_counter() - start
    cells = np.asarray([source["cell"] for source in sources])
    hidden, work = np.zeros(len(sources), dtype=bool), Counter()
    start = time.perf_counter()
    for cell in np.unique(cells):
        packet = [item for item in occluders if cells[item[1]] == cell]
        tree = BoundsHierarchy(bounds, known=known & (cells == cell))
        result = tree.select(packet, protected=protected)
        hidden |= result.hidden
        work.update(result.work)
    return (
        {
            "whole": np.zeros(len(sources), dtype=bool),
            "cells": hidden,
            "joint": joint.hidden,
        },
        {
            "joint_build_s": build_s,
            "joint_query_s": joint_s,
            "joint_work": joint.work,
            "cells_build_and_query_s": time.perf_counter() - start,
            "cells_work": dict(work),
        },
    )


def render(triangles, view):
    points = triangles.reshape(-1, 3)
    projected, z = project(points, view)
    return raster(projected, z, np.arange(len(points)).reshape(-1, 3), 1280, 720)


def images(native, sources, masks, seed_geometry, output, *, vertex_recipe="float64"):
    """Control meshes are emitted after every selection decision has been made."""
    full, omissions = [], Counter()
    for index, source in enumerate(sources):
        triangles, reasons = (
            seed_geometry[index]
            if index in seed_geometry
            else emit_roof(source, vertex_recipe=vertex_recipe)
        )
        full.append(triangles)
        omissions.update(reasons)
        if len(triangles) and source["bounds"] is not None:
            low, high = source["bounds"]["min"], source["bounds"]["max"]
            if (triangles < low).any() or (triangles > high).any():
                if any(mask[index] for mask in masks.values()):
                    raise ValueError("culled source geometry escapes native envelope")
                omissions["retained_source_geometry_escapes_native_envelope"] += 1
    models = {
        name: np.concatenate([part for i, part in enumerate(full) if not mask[i]])
        for name, mask in masks.items()
    }
    return {
        "images": render_models(native, sources, models, output),
        "roof_triangles": {name: len(model) for name, model in models.items()},
        "omissions": dict(omissions),
        "sources_retained": {name: int((~mask).sum()) for name, mask in masks.items()},
    }


def render_models(native, sources, models, output):
    """Compare independently emitted models with one metadata-defined camera."""
    scene = {"anchor_ecef": native["anchor_ecef"], **native["view"]}
    camera_targets = np.array(
        [
            (np.asarray(source["bounds"]["min"]) + source["bounds"]["max"]) * 0.5
            for source in sources
            if source["bounds"] is not None
        ]
    )
    rows = []
    for movement in (0, 1):
        for turn in range(4):
            view = camera(scene, camera_targets, 1280, 720, turn * np.pi / 2)
            eye, basis, focal, width, height = view
            view = (
                eye + movement * np.array([1.0, -1.0, 0.5]),
                basis,
                focal,
                width,
                height,
            )
            control = render(models["whole"], view)
            row = {
                "movement": movement,
                "quarter_turn": turn,
                "eye": view[0].tolist(),
                "basis": basis.tolist(),
                "covered_pixels": int((control > 0).sum()),
            }
            for name, model in models.items():
                depth = control if name == "whole" else render(model, view)
                changed = int(np.count_nonzero(depth != control))
                row[f"{name}_changed_depth_pixels"] = changed
                grey = np.where(
                    depth > 0, 40 + 200 * np.clip(depth * 2000, 0, 1), 15
                ).astype(np.uint8)
                Image.fromarray(grey).save(
                    output / f"move-{movement}-turn-{turn}-{name}.png"
                )
                if changed:
                    raise ValueError("selected roof reference changes raster depth")
            rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        output.exists()
        or output == temporary
        or not output.is_relative_to(temporary)
        or output.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    native = json.loads(args.input.read_bytes())
    sources = native["sources"]
    bounds = np.array(
        [
            [source["bounds"]["min"], source["bounds"]["max"]]
            if source["bounds"] is not None
            else [[np.nan] * 3] * 2
            for source in sources
        ]
    )
    known = np.isfinite(bounds).all(axis=(1, 2))
    eye = np.asarray(native["view"]["eye_ecef"]) - native["anchor_ecef"]
    observer = np.array([eye - 2, eye + 2])
    geometry, packet, protected, seed_counts = seeds(
        sources, bounds, known, eye, observer
    )
    masks, work = selections(sources, bounds, known, packet, protected)
    output.mkdir(parents=True)
    report = {
        "scope": "Roof-only Float64 software reference using actual native shape plans. Accepted explicitly opaque experiment roof seeds are emitted first and retained. Remaining roofs emitted after selection. No native mesh/Fine/material parity, full world, shadow/reflection/interaction demand or device budget acceptance. Four quarter-turn images plus bounded movement sample the analytically certified entire observer box; not a full runtime 360-degree gate. All unknown/invalid/nonplanar source records remain accounted for.",
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "input": str(args.input.resolve()),
        "code_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "building_roof_domains.py",
                "building_roof_positions.py",
            )
        },
        "observer_box": observer.tolist(),
        "source_records": len(sources),
        "unknown_source_bounds": int((~known).sum()),
        "seed_counts": dict(seed_counts),
        "selection": work,
        **images(native, sources, masks, geometry, output),
        "hidden_source_indices": {
            name: np.flatnonzero(mask).tolist() for name, mask in masks.items()
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: report[key]
                for key in (
                    "seed_counts",
                    "roof_triangles",
                    "sources_retained",
                    "omissions",
                )
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
