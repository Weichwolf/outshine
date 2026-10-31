"""Whole original cohort versus constructive material domains including undersides.

This is a source/word comparison and saved construction diagnostic. Different
triangulations still need continuous support and actual native image checks;
corner membership alone is not surface, material or World admission.
"""

import argparse
import hashlib
import json
import subprocess
import time
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
import shapely
from complete_source_patches import complete_shape_patches
from experiment_output import create_output
from parent_surface_inputs import open_parent
from source_patch_inputs import read_bound_sources
from welded_source_patch import source_corners

ROOT = Path(__file__).resolve().parents[3]
FIELDS = ("positions", "normals", "uv", "uv1", "colours")


def rows(corners):
    return np.ascontiguousarray(
        np.concatenate([corners[key] for key in FIELDS], axis=-1), dtype="<f4"
    )


def run(args):
    files = [
        Path(__file__).with_name(name)
        for name in (
            "source_patch_recipe_study.py",
            "source_patch_inputs.py",
            "complete_source_patches.py",
            "welded_source_patch.py",
            "source_material_patches.py",
            "source_material_domains.py",
            "source_shading_fields.py",
            "column_roof_patches.py",
            "source_surface_patches.py",
            "source_plane_mesh.py",
            "building_sample_originals.py",
            "native_building_cell_reference.py",
            "compare_building_originals.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    create_output(args.output)
    began = time.process_time()
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    preparation_cpu = time.process_time() - began
    original = np.c_[
        source["positions"],
        np.concatenate(
            [
                np.concatenate([part[key] for key in FIELDS[1:]], axis=1)
                for part in source["attributes"]
            ]
        ),
    ]
    by_source = defaultdict(list)
    for index, identity in enumerate(context["triangle_binding"][:, 1]):
        by_source[int(identity)].append(index)
    selected = set(map(int, context["source_ordinals"]))
    seen, results, failures = set(), [], Counter()
    began = time.process_time()
    sampled = False
    with open_parent(context["source_archive"]) as parent:
        for _cell, identities, surfaces in parent.iter_forms(
            context["binding"]["cells"]
        ):
            for identity, surface in zip(identities, surfaces, strict=True):
                identity = int(identity)
                if identity not in selected or identity in seen:
                    raise ValueError("Exact whole cohort source order required")
                seen.add(identity)
                face_ids = np.array(by_source[identity], np.int64)
                old_faces = source["indices"][face_ids]
                row = {
                    "prepared_source_ordinal": identity,
                    "original_triangles": len(face_ids),
                }
                try:
                    if surface is None or not surface["shapes"]:
                        raise ValueError("Original whole source form unavailable")
                    patches = [
                        patch
                        for part, shape in enumerate(surface["shapes"])
                        for patch in complete_shape_patches(
                            surface,
                            shape,
                            identity,
                            part,
                            context["render_basis"],
                            np.zeros(3),
                        )
                    ]
                    candidate, work = source_corners(patches, context["render_basis"])
                except (ValueError, shapely.errors.GEOSException) as error:
                    failures[str(error)] += 1
                    results.append(
                        row
                        | {
                            "candidate_constructed": False,
                            "original_retained": True,
                            "reason": str(error),
                        }
                    )
                    continue
                before = np.ascontiguousarray(original[old_faces], dtype="<f4")
                after = rows(candidate)
                old_vertices = np.unique(before.reshape(-1, 14).view("V56").ravel())
                new_vertices = after.reshape(-1, 14).view("V56").ravel()
                same = np.array_equal(
                    np.sort(before.reshape(-1, 42).view("V168").ravel()),
                    np.sort(after.reshape(-1, 42).view("V168").ravel()),
                )
                row.update(
                    candidate_constructed=True,
                    original_retained=True,
                    candidate_triangles=len(after),
                    source_domains=len(patches),
                    candidate_corners=3 * len(after),
                    candidate_corner_word_matches=int(
                        np.isin(new_vertices, old_vertices).sum()
                    ),
                    same_ordered_corner_triangle_multiset=same,
                    work=work,
                )
                results.append(row)
                if not sampled:
                    used, inverse = np.unique(old_faces, return_inverse=True)
                    np.savez_compressed(
                        args.output / "sample.npz",
                        original_vertices=original[used],
                        original_faces=inverse.reshape(-1, 3),
                        candidate_vertices=after.reshape(-1, 14),
                        candidate_faces=np.arange(3 * len(after)).reshape(-1, 3),
                        candidate_material=candidate["material"],
                    )
                    (args.output / "sample.json").write_text(
                        json.dumps(row, indent=2) + "\n"
                    )
                    sampled = True
        form_reads, form_bytes = parent.form_reads, parent.form_bytes
    if seen != selected:
        raise ValueError("Every original cohort source requires a result")
    constructed = [row for row in results if row["candidate_constructed"]]
    summary = {
        "whole_sources": len(results),
        "original_triangles": len(source["indices"]),
        "constructed_sources": len(constructed),
        "fallback_reasons": dict(failures),
        "original_triangles_of_constructed_sources": sum(
            row["original_triangles"] for row in constructed
        ),
        "constructed_triangles": sum(row["candidate_triangles"] for row in constructed),
        "ordered_corner_exact_sources": sum(
            row["same_ordered_corner_triangle_multiset"] for row in constructed
        ),
        "candidate_corners": sum(row["candidate_corners"] for row in constructed),
        "candidate_corner_word_matches": sum(
            row["candidate_corner_word_matches"] for row in constructed
        ),
        "native_rule_pruned_degenerate_triangles": sum(
            row["work"]["native_rule_pruned_degenerate_triangles"]
            for row in constructed
        ),
        "whole_input_binding_cpu_s": preparation_cpu,
        "whole_form_decode_construction_and_word_comparison_cpu_s": time.process_time()
        - began,
        "form_reads": form_reads,
        "form_bytes": form_bytes,
        "scope": "Structural one-run evidence, not qualified cost comparison. All originals remain authoritative; no admission from counts, corner membership or exact finite subsets.",
    }
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "input_provenance": provenance,
        "summary": summary,
        "sources": results,
        "scope": __doc__,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
