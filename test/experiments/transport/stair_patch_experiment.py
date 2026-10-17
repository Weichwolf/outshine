"""Convert verified local fields to exact independently addressed resident patches."""

import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_module_cache import parent_report
from stair_module_inputs import cached, read_arrays
from stair_patch_inputs import (
    argument_parser,
    frame_for,
    fresh,
    from_field,
    query_points,
    save_manifest,
    save_patch,
    whole,
)
from stair_terrain_experiment import frozen


def compare(view, layout, field):
    actual = whole(view)
    xyz = np.c_[layout["xy"], field["heights"]][layout["faces"]]
    if actual.mesh.vertices[actual.mesh.triangles].tobytes() != xyz.tobytes():
        raise ValueError("patch view differs from authoritative whole XYZ triangles")
    ground = layout["ground"]
    constructed = np.zeros(len(ground), bool)
    roles = field["constructed"]
    if len(roles) == len(ground):
        constructed[:] = roles
    else:
        constructed[~ground] = roles
    if not np.array_equal(actual.face_data["ground"], ground) or not np.array_equal(
        actual.face_data["constructed"], constructed
    ):
        raise ValueError("patch view changed authoritative ground/constructed roles")
    return {"exact_XYZ_facets": len(xyz), "exact_face_attributes": True}


def append_patch(output, state, view, refs, path, fixture, history):
    report, _recipe = parent_report(path, fixture, state, history)
    chosen = read_arrays(path / "selection.npz")["old_selected_faces"]
    if chosen.dtype.kind != "b" or chosen.shape != (len(refs),):
        raise ValueError("original local removal witness changed")
    removals = {}
    for (name, index), selected in zip(refs, chosen, strict=True):
        if selected:
            removals.setdefault(name, []).append(index)
    patch = from_field(
        read_arrays(path / "local-layout.npz"),
        read_arrays(path / "local-field.npz"),
        view.patches[0].frame,
    )
    view, note = view.replace(patch, removals)
    row = save_patch(output, len(history) + 1, patch)
    row["removals"] = removals
    refs = [r for r, used in zip(refs, chosen, strict=True) if not used]
    refs.extend((patch.patch_id, i) for i in range(len(patch.mesh.triangles)))
    proof = compare(
        view,
        read_arrays(path / "world-layout.npz"),
        read_arrays(path / "world-field.npz"),
    )
    history.append(
        {
            "report_sha256": digest(path / "comparison.json"),
            "fixture_sha256": digest(fixture),
        }
    )
    state.update(
        report={"candidate_source_ids": report["candidate_source_ids"]},
        baseline_report_sha256=digest(path / "comparison.json"),
        before_display_path=path / "after-display.npz",
    )
    return view, refs, row, {**note, **proof}


def run(args):
    commit = frozen()
    fresh(args.output)
    if not 1 <= len(args.cached_module) <= 32:
        raise ValueError(
            "one to 32 explicit already verified research modules required"
        )
    began = time.perf_counter()
    ref = {
        "reference_fixture": args.reference.name,
        "reference_fixture_sha256": digest(args.reference),
    }
    state = cached(args.base, args.supports, ref)
    state["before_display_path"] = args.supports / "after-display.npz"
    frame = frame_for(state["support_report"]["origin_lat_lon"])
    root = from_field(
        state["plan"], read_arrays(args.base / "membrane-field.npz"), frame
    )
    from outshine_ref.geometry.surface_patches import SurfacePatches

    view = SurfacePatches(root)
    args.output.mkdir(parents=True)
    (args.output / "assets").mkdir()
    patches, history, proofs = [save_patch(args.output, 0, root)], [], []
    refs = [(root.patch_id, i) for i in range(len(root.mesh.triangles))]
    for path, fixture in args.cached_module:
        view, refs, row, proof = append_patch(
            args.output, state, view, refs, Path(path), Path(fixture), history
        )
        patches.append(row)
        proofs.append(proof)
    result = whole(view)
    np.savez_compressed(args.output / "query-cases.npz", xy=query_points(view))
    np.savez_compressed(
        args.output / "flat-reference.npz",
        vertices=result.mesh.vertices,
        faces=result.mesh.triangles,
        **result.face_data,
    )
    manifest = {
        "version": 1,
        "commit": commit,
        "frame": frame.identifier,
        "origin_lat_lon": state["support_report"]["origin_lat_lon"],
        "patches": patches,
        "query_cases_sha256": digest(args.output / "query-cases.npz"),
        "flat_reference_sha256": digest(args.output / "flat-reference.npz"),
        "source_ids": state["report"]["candidate_source_ids"],
        "source_reports": history,
        "proofs": proofs,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Exact resident finite surface patches from already verified products. No source decode, new network growth, native storage/streaming/LOD or complete-world claim.",
    }
    save_manifest(args.output, manifest)
    print(
        json.dumps({k: manifest[k] for k in ("commit", "seconds", "peak_rss_bytes")}),
        flush=True,
    )
    return view, manifest


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(argument_parser().parse_args())
