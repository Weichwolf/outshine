"""Actual bounded prototype: joint angular sampling versus positive column reduction."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_window_catalog import family_geometry_for_seed, prototypes
from foliage_selection import directions
from joint_basis_reduction import errors, reduce_fields
from joint_light_measure import JointMeasure
from needle_area_light import integrate
from needle_joint_contacts import prepare_source
from needle_response_study import source_digest
from outshine_ref.vegetation import NeedlePattern
from plant_atlas import projection


def view_light_knots(count=32):
    rows = [np.array([0.0, 0, 1]), np.array([0.0, 0, -1])]
    rings = [(2, 4), (10, 4), (40, 8), (90, 8), (140, 6)]
    if count == 64:
        rings = [(1, 4), (3, 8), (10, 8), (25, 10), (50, 10), (85, 12), (130, 10)]
    elif count != 32:
        raise ValueError("32 or 64 bounded light knots required")
    for degrees, count in rings:
        theta = np.deg2rad(degrees)
        azimuth = np.arange(count) * 2 * np.pi / count
        rows.extend(
            np.c_[
                np.sin(theta) * np.cos(azimuth),
                np.sin(theta) * np.sin(azimuth),
                np.full(count, np.cos(theta)),
            ]
        )
    return np.array(rows)


def view_held_queries():
    theta = np.deg2rad([1, 5, 22, 65, 110, 165])
    azimuth = 0.4 + 1.3 * np.arange(6)
    return np.r_[
        [[0.0, 0, 1], [0.0, 0, -1]],
        np.c_[
            np.sin(theta) * np.cos(azimuth),
            np.sin(theta) * np.sin(azimuth),
            np.cos(theta),
        ],
    ]


def freeze_source(models, catalog, output, capacity):
    metadata = json.loads((catalog / "catalog.json").read_text())
    original = json.loads((catalog / "source.json").read_text())
    info = next(row for row in metadata.values() if row["needles"] == capacity)
    meshes, paths = prototypes(models, json.loads((models / "shoot.json").read_text()))
    if capacity * (len(meshes[0][0]["faces"]) + 36) > 250000:
        raise ValueError("bounded prototype before expansion")
    geometry, facts = family_geometry_for_seed(
        original["canonical_seed"],
        NeedlePattern.from_recipe(info["pattern"]),
        meshes,
        capacity,
    )
    output.mkdir()
    np.savez_compressed(output / "physical.npz", **geometry)
    paths += [models / "shoot.json", catalog / "catalog.json", catalog / "source.json"]
    (output / "source.json").write_text(
        json.dumps(
            facts | {"input_sha256": {str(p): source_digest(p) for p in paths}},
            indent=2,
        )
        + "\n"
    )
    return prepare_source(output / "physical.npz")


def run(models, catalog, output, order, capacity, angles="object", knot_count=32):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source, scene = freeze_source(models, catalog, output / "source", capacity)
    views = np.array([[1.0, 0, 0.35], [0.2, 0.9, -0.39], [-0.62, 0.41, 0.67]])
    views /= np.linalg.norm(views, axis=1)[:, None]
    random = np.random.default_rng(643).normal(size=(6, 3))
    random /= np.linalg.norm(random, axis=1)[:, None]
    held = np.array([np.r_[view[None], -view[None], random] for view in views])
    knots = directions(knot_count)
    if angles == "view":
        knots = view_light_knots(knot_count)
        held = np.array([view_held_queries()] * len(views))
    arrays = {"knots": knots, "heldout": held, "views": views}
    joint = []
    coverage = []
    records = []
    for index, view in enumerate(views):
        queries = np.r_[knots, held[index]]
        if angles == "view":
            queries = queries @ projection(source["vertices"], view)[0].T
            queries[0], queries[1] = view, -view
            queries[len(knots)], queries[len(knots) + 1] = view, -view
        arrays[f"directions{index}"] = queries
        measure = JointMeasure(
            source, scene, view, queries, size=16, max_work=8_000_000
        )
        before = dict(scene.work)
        began = time.perf_counter()
        direct, work = integrate(
            source,
            scene,
            view,
            [view],
            size=16,
            order=order,
            max_samples=measure.max_points,
            contact_measure=measure,
        )
        np.testing.assert_array_equal(measure.joint[len(knots)], direct[2].ravel())
        assert np.max(measure.joint - measure.unshadowed) < 2e-12
        assert np.max(measure.unshadowed - measure.coverage) < 2e-12
        arrays[f"joint{index}"] = measure.joint
        arrays[f"unshadowed{index}"] = measure.unshadowed
        arrays[f"independent{index}"] = measure.independent()
        arrays[f"coverage{index}"] = measure.coverage
        joint.append(measure.joint)
        coverage.append(measure.coverage)
        record = {
            "view_index": index,
            "wall_s": time.perf_counter() - began,
            "source_work": work,
            "measure_work": measure.work,
            "rays": {k: scene.work[k] - before[k] for k in before},
            "independent_error": errors(measure.independent(), measure.joint),
            "unshadowed_error": errors(measure.unshadowed, measure.joint),
            "field_bytes": sum(
                a.nbytes
                for a in (
                    measure.coverage,
                    measure.joint,
                    measure.unshadowed,
                    measure.visible_mass,
                    measure.positive_mass,
                )
            ),
        }
        records.append(record)
        print(
            json.dumps(
                {
                    "view": index,
                    "seconds": record["wall_s"],
                    "rays": record["rays"]["rays"],
                }
            ),
            flush=True,
        )
    np.savez_compressed(output / "fields.npz", **arrays)
    reduction = reduce_fields(np.array(joint), np.array(coverage), knots, held, output)
    result = {
        "schema": "joint-basis-pilot-2",
        "knot_count": len(knots),
        "heldout_lights": "six independent plus two pole controls"
        if angles == "view"
        else "eight independent",
        "capacity": capacity,
        "order": order,
        "angular_coordinates": angles,
        "views": records,
        "reduction": reduction,
        "source_sha256": source_digest(output / "source/physical.npz"),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "same finite opaque direct contacts; angular/rank approximation measured separately, no PBR/R/T/default admission",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("models", "catalog", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--order", type=int, choices=(2, 4), default=2)
    parser.add_argument("--capacity", type=int, choices=(2, 32, 256), default=32)
    parser.add_argument("--angles", choices=("object", "view"), default="object")
    parser.add_argument("--knots", type=int, choices=(32, 64), default=32)
    args = parser.parse_args()
    run(
        args.models,
        args.catalog,
        args.output,
        args.order,
        args.capacity,
        args.angles,
        args.knots,
    )
