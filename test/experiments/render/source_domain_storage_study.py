"""Whole-cohort encoded domain storage, including recipes and original fallbacks.

No native cache format or World admission is selected. Compare direct Float64
controls with a raw-word dictionary, then restore only requested cached domains.
"""

import argparse
import hashlib
import json
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from complete_source_patches import complete_shape_patches
from experiment_output import create_output
from parent_surface_inputs import open_parent
from source_domain_buffer import pack, restore_source
from source_patch_inputs import read_bound_sources
from source_patch_product import FIELDS, read_product
from source_patch_support import primitive_words
from welded_source_patch import source_corners

ROOT = Path(__file__).resolve().parents[3]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def row_words(corners):
    return np.ascontiguousarray(np.concatenate([corners[k] for k in FIELDS], axis=-1))


def measure_pack(groups, basis, dictionary, path):
    tracemalloc.start()
    began = time.process_time()
    arrays = pack(groups, basis, dictionary=dictionary)
    cpu = time.process_time() - began
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    np.savez(path, **arrays)
    report = {
        "array_bytes": sum(a.nbytes for a in arrays.values()),
        "stored_npz_bytes": path.stat().st_size,
        "arrays": {
            key: {"shape": list(a.shape), "bytes": a.nbytes}
            for key, a in arrays.items()
        },
        "single_instrumented_pack_cpu_s": cpu,
        "scoped_tracemalloc_current_bytes": current,
        "scoped_tracemalloc_peak_bytes": peak,
        "sha256": sha(path),
    }
    return report


def run(args):
    producer = json.loads((args.input / "report.json").read_text())
    names = set(producer["source_sha256"]) | {
        "source_domain_buffer.py",
        "source_domain_storage_study.py",
        "source_patch_support.py",
    }
    files = [Path(__file__).with_name(name) for name in sorted(names)]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    hashes = {path.name: sha(path) for path in files}
    create_output(args.output)
    original, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    if any(
        producer["input_provenance"][key] != provenance[key]
        for key in ("source_binding_sha256", "source_archive_sha256")
    ):
        raise ValueError("Same actual source/frame cohort required")
    selected = set(map(int, context["source_ordinals"]))
    groups, failures, recipes, seen = [], [], [], set()
    recipe_dir = args.output / "recipes"
    recipe_dir.mkdir()
    began = time.process_time()
    with open_parent(context["source_archive"]) as parent:
        for cell, identities, surfaces in parent.iter_forms(
            context["binding"]["cells"]
        ):
            raw = parent.member(parent.forms[cell])
            file = recipe_dir / f"{cell}.bin"
            file.write_bytes(raw)
            source_file = recipe_dir / f"{cell}.sources.npy"
            np.save(source_file, np.asarray(identities, dtype="<i4"))
            recipes.append(
                {
                    "cell": cell,
                    "file": file.name,
                    "bytes": len(raw),
                    "sha256": sha(file),
                    "source_file": source_file.name,
                    "source_bytes": identities.size * 4,
                    "source_sha256": sha(source_file),
                }
            )
            for identity, surface in zip(identities, surfaces, strict=True):
                identity = int(identity)
                if identity not in selected or identity in seen:
                    raise ValueError("Actual whole source order required")
                seen.add(identity)
                try:
                    if surface is None or not surface["shapes"]:
                        raise ValueError("Whole source recipe unavailable")
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
                    groups.append((identity, surface, patches))
                except (ValueError, shapely.errors.GEOSException) as error:
                    failures.append({"source": identity, "reason": str(error)})
        form_bytes, form_reads = parent.form_bytes, parent.form_reads
    construction_cpu = time.process_time() - began
    if seen != selected:
        raise ValueError("Whole source cohort required")
    (recipe_dir / "index.json").write_text(json.dumps(recipes, indent=2) + "\n")
    fallback_ids = np.array([row["source"] for row in failures], np.int32)
    fallback_faces = np.flatnonzero(
        np.isin(context["triangle_binding"][:, 1], fallback_ids)
    )
    vertices = np.c_[
        original["positions"],
        np.concatenate(
            [
                np.concatenate([part[key] for key in FIELDS[1:]], axis=1)
                for part in original["attributes"]
            ]
        ),
    ]
    materials = np.concatenate(
        [np.full(p["triangles"], p["material"], np.int32) for p in original["parts"]]
    )
    fallback = {
        "corners": vertices[original["indices"][fallback_faces]],
        "material": materials[fallback_faces],
        "actual_triangle_binding": context["triangle_binding"][fallback_faces].astype(
            "<i4"
        ),
    }
    np.savez(args.output / "fallback.npz", **fallback)
    reports = {}
    for dictionary, mode in (
        (False, "direct"),
        (True, "dictionary"),
        ("scalar", "scalar"),
    ):
        reports[mode] = measure_pack(
            groups, context["render_basis"], dictionary, args.output / (mode + ".npz")
        )
    mode = min(reports, key=lambda key: reports[key]["array_bytes"])
    with np.load(args.output / (mode + ".npz"), allow_pickle=False) as stored:
        arrays = {key: stored[key] for key in stored.files}
    began = time.process_time()
    controls = 0
    for identity, surface, patches in groups:
        restored = restore_source(arrays, identity, surface)
        if len(restored) != len(patches):
            raise ValueError("Every complete cached patch must survive")
        for before, after in zip(patches, restored, strict=True):
            if (
                before.geometry.domain.wkb != after.geometry.domain.wkb
                or not np.array_equal(
                    np.asarray(before.local_origin).view("u8"),
                    after.local_origin.view("u8"),
                )
                or not np.array_equal(
                    before.local_axes.view("u8"), after.local_axes.view("u8")
                )
                or (before.part, before.side, before.gable, before.orientation)
                != (after.part, after.side, after.gable, after.orientation)
            ):
                raise ValueError("Cached complete construction controls changed")
            controls += 1
    restore_cpu = time.process_time() - began
    identity, surface, patches = next(
        group for group in groups if group[0] == args.sample_source
    )
    began = time.process_time()
    restored = restore_source(arrays, identity, surface)
    selected_cpu = time.process_time() - began
    actual, work = source_corners(restored, context["render_basis"])
    parts = read_product(
        args.input / "candidate",
        producer["diagnostic_native_candidate"],
        context["source_ordinals"],
    )
    expected, roles = [], []
    for part, values, ids in parts:
        selected_faces = values["indices"][ids == identity]
        expected.extend(row_words(values)[selected_faces])
        roles.extend([part["material"]] * len(selected_faces))
    if not np.array_equal(
        primitive_words(np.asarray(expected), np.asarray(roles)),
        primitive_words(row_words(actual), actual["material"]),
    ):
        raise ValueError("Selected cached source differs from bound native candidate")
    np.savez(args.output / "sample.npz", **actual)
    recipe_bytes = sum(row["bytes"] + row["source_bytes"] for row in recipes)
    fallback_bytes = sum(a.nbytes for a in fallback.values())
    total = reports[mode]["array_bytes"] + recipe_bytes + fallback_bytes
    original_bytes = sum(
        p["vertices"] * 56 + p["triangles"] * 12 for p in original["parts"]
    )
    summary = {
        "whole_sources": len(seen),
        "constructed_sources": len(groups),
        "complete_patches": controls,
        "whole_fallbacks": failures,
        "fallback_triangles": len(fallback_faces),
        "selected_storage_mode": mode,
        "domain_array_bytes": reports[mode]["array_bytes"],
        "retained_raw_recipe_and_source_id_bytes": recipe_bytes,
        "fallback_array_bytes": fallback_bytes,
        "total_encoded_payload_bytes": total,
        "original_native_array_bytes": original_bytes,
        "encoded_payload_reduction_fraction": 1 - total / original_bytes,
        "total_stored_files_bytes": sum(p.stat().st_size for p in recipe_dir.iterdir())
        + (args.output / "fallback.npz").stat().st_size
        + reports[mode]["stored_npz_bytes"],
        "single_domain_construction_recipe_copy_cpu_s": construction_cpu,
        "single_all_source_restore_and_control_comparison_cpu_s": restore_cpu,
        "form_reads": form_reads,
        "form_bytes": form_bytes,
        "sample_source": identity,
        "sample_cached_domains_restored": len(restored),
        "sample_source_restore_cpu_s": selected_cpu,
        "sample_native_triangles": len(actual["material"]),
        "sample_bound_candidate_primitive_words_exact": True,
        "sample_native_reconstruction_work": work,
        "scope": "Encoded payload comparison, not runtime resident objects/peak, native devices or World admission. Original production arrays remain authoritative. All raw recipes, source IDs and whole fallback corners/bindings are included. Packed controls reproduce the diagnostic construction, not the original native surface. Source selection restores only requested cached domains; it does not prove camera culling or material/raster validity. Timings are single development runs; scoped tracemalloc excludes native GEOS allocations and prior input/recipe objects. Total allocation traffic and complete copied bytes remain unmeasured.",
    }
    if hashes != {path.name: sha(path) for path in files}:
        raise ValueError("Measured sources changed during run")
    report = {
        "commit": commit,
        "source_sha256": hashes,
        "producer_report_sha256": sha(args.input / "report.json"),
        "input_provenance": provenance,
        "summary": summary,
        "packing_alternatives": reports,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-source", type=int, default=1147)
    run(parser.parse_args())
