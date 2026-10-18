"""Compare pre-emission original roof-part bounds against retained roof seeds."""

import argparse
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from building_roof_domains import planar_roof_domains
from building_roof_positions import roof_positions
from building_roof_selection import seeds
from outshine_ref.geometry.occlusion import classify_boxes
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy


def part_support(piece, surface, *, vertex_recipe="float64"):
    """Bound contour vertices before triangulation; emission must check containment."""
    xy = shapely.get_coordinates(piece["domain"])
    plane = piece["plane_enu"]
    local = np.column_stack((xy, xy @ plane[:2] + plane[2]))
    basis = np.asarray([surface[key] for key in ("east", "north", "up")])
    world = roof_positions(local, plane, surface, recipe=vertex_recipe)
    scale = np.abs(surface["origin"]) + np.abs(local) @ np.abs(basis)
    rounding_allowance = 64 * np.finfo(float).eps * np.maximum(scale.max(axis=0), 1)
    return world, np.array(
        [
            np.nextafter(world.min(axis=0) - rounding_allowance, -np.inf),
            np.nextafter(world.max(axis=0) + rounding_allowance, np.inf),
        ]
    )


def parts(sources, *, vertex_recipe="float64"):
    rows, omissions = [], Counter()
    for index, source in enumerate(sources):
        surface = source["surface"]
        if surface is None:
            omissions["unknown_source_surface"] += 1
            continue
        for shape_index, shape in enumerate(surface["shapes"]):
            try:
                pieces = planar_roof_domains(shape)
            except ValueError as error:
                omissions[str(error)] += 1
                continue
            if not pieces:
                omissions["no_original_planar_domains"] += 1
            for piece in pieces:
                if piece["domain"].area <= 0:
                    omissions["nonareal_domain"] += 1
                    continue
                support, bounds = part_support(
                    piece, surface, vertex_recipe=vertex_recipe
                )
                rows.append(
                    {
                        "source_index": index,
                        "shape": shape_index,
                        "kind": piece["kind"],
                        "index": piece["index"],
                        "bounds": bounds,
                        "support": support,
                        "piece": piece,
                    }
                )
    return rows, omissions


def contour_selection(rows, packet, protected, *, max_tests=2_000_000):
    """Use original contour supports instead of admitting the inflated world AABB."""
    if type(max_tests) is not int or not 0 <= max_tests <= 100_000_000:
        raise ValueError("bounded nonnegative contour work budget required")
    bounds = np.asarray([row["bounds"] for row in rows])
    hidden, work = np.zeros(len(rows), dtype=bool), Counter()
    work.update(
        box_tests=0,
        contour_point_tests=0,
        contour_candidates=0,
        budget_limit=max_tests,
        budget_exhausted=False,
    )
    for cone, _ in packet:
        for start in range(0, len(rows), 512):
            indices = np.arange(start, min(start + 512, len(rows)))
            indices = indices[~hidden[indices] & ~protected[indices]]
            if not len(indices):
                continue
            if (
                work["box_tests"] + work["contour_point_tests"] + len(indices)
                > max_tests
            ):
                work["budget_exhausted"] = True
                return hidden, dict(work)
            inside, outside = classify_boxes(bounds[indices], cone)
            hidden[indices[inside]] = True
            work["box_tests"] += len(indices)
            indices = indices[~inside & ~outside]
            if not len(indices):
                continue
            lengths = np.array([len(rows[index]["support"]) for index in indices])
            if (
                work["box_tests"] + work["contour_point_tests"] + int(lengths.sum())
                > max_tests
            ):
                work["budget_exhausted"] = True
                return hidden, dict(work)
            points = np.concatenate([rows[index]["support"] for index in indices])
            flags = np.zeros(len(points), dtype=bool)
            for first in range(0, len(points), 512):
                values = points[first : first + 512]
                flags[first : first + len(values)] = classify_boxes(
                    np.stack((values, values), axis=1), cone
                )[0]
            all_inside = np.minimum.reduceat(flags, np.r_[0, lengths.cumsum()[:-1]])
            hidden[indices[all_inside]] = True
            work["contour_candidates"] += len(indices)
            work["contour_point_tests"] += len(points)
    return hidden, dict(work)


def run(path, output):
    native = json.loads(path.read_bytes())
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
    _, packet, protected_sources, counts = seeds(sources, bounds, known, eye, observer)
    rows, omissions = parts(sources)
    target_bounds = np.array([row["bounds"] for row in rows])
    protected = np.array([protected_sources[row["source_index"]] for row in rows])
    tree = BoundsHierarchy(target_bounds, known=np.ones(len(rows), dtype=bool))
    selected = tree.select([(cone, None) for cone, _ in packet], protected=protected)
    contour_hidden, contour_work = contour_selection(rows, packet, protected)
    hidden = [
        {key: row[key] for key in ("source_index", "shape", "kind", "index")}
        for row, flag in zip(rows, selected.hidden, strict=True)
        if flag
    ]
    report = {
        "scope": "Pre-emission roof-part bound pilot, not a complete render/geometry/rounding/cost proof. Source records stay intact. The Float64 bound allowance needs actual post-selection emission containment checks before adoption. No full detail meshes are used as selection input; only a bounded retained roof seed set is emitted first.",
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "source_records": len(sources),
        "parts": len(rows),
        "seed_counts": dict(counts),
        "omissions": dict(omissions),
        "hidden_parts": hidden,
        "work": selected.work,
        "contour_hidden_parts": [
            {key: row[key] for key in ("source_index", "shape", "kind", "index")}
            for row, flag in zip(rows, contour_hidden, strict=True)
            if flag
        ],
        "contour_work": contour_work,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "parts": len(rows),
                "hidden_parts": len(hidden),
                "contour_hidden": int(contour_hidden.sum()),
                "work": selected.work,
                "contour_work": contour_work,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    target.mkdir(parents=True)
    run(args.input, target)
