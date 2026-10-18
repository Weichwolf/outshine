"""Same per-ID mesh detail across all form variants, before diagnostic expansion."""

import argparse
import json
from pathlib import Path

import numpy as np
from crown_demand import projected_spans
from crown_draw_batches import posed_bounds
from demand import Camera
from shoot_form import ShootForm
from shoot_form_probe import TRAITS
from spatial_group_source import load_source


def matched_detail(source, forms, camera, threshold_px=96.0):
    if not np.isfinite(threshold_px) or threshold_px <= 0 or not 1 <= len(forms) <= 4:
        raise ValueError(
            "bounded comparison forms and positive finite pixel threshold required"
        )
    fine = np.zeros(len(source.crown.variants), bool)
    prototypes = [b.mesh(32, 4) for b in source.blades]
    for form in forms:
        if form.base.identity != source.identity:
            raise ValueError("one unchanged physical inventory required")
        selected = camera.visible(form.lower, form.upper)
        poses = form.evaluate(selected, max_leaves=100000)
        for variant, product in enumerate(prototypes):
            chosen = poses["variants"] == variant
            if not chosen.any():
                continue
            low, high = posed_bounds(
                product, poses["origins"][chosen], poses["frames"][chosen]
            )
            needed = camera.visible(low, high) & (
                projected_spans(camera, low, high) >= threshold_px
            )
            fine[poses["ids"][chosen][needed]] = True
    fine.setflags(write=False)
    return fine


def prepare(inputs, study, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    source = load_source(inputs)
    configuration = json.loads((study / "scene.json").read_text())
    forms = [
        ShootForm.load(study / f"{name}.npz", expected_source=source.identity)
        for name in TRAITS
    ]
    masks, records = {}, []
    for view in ("whole", "near"):
        c = configuration[view]
        camera = Camera(
            c["eye"],
            c["target"],
            fov_y=c["fov_y_deg"],
            width=2560,
            height=1440,
            near=0.05,
            far=1000,
        )
        fine = matched_detail(source, forms, camera)
        masks[view] = fine
        records.append(
            {
                "view": view,
                "fine_leaf_ids": int(fine.sum()),
                "mask_array_bytes": fine.nbytes,
            }
        )
    np.savez_compressed(output, **masks)
    report = {
        "source_identity": source.identity,
        "forms": [f.identity for f in forms],
        "threshold_bound_px": 96.0,
        "cases": records,
        "scope": "Experimental four-form union of visible projected fine-prototype AABBs. Same original leaf ID uses the same mesh in all comparisons, retaining equal triangulated physical inventory. This matched counterfactual policy is not a runtime proposal or a general LOD quality certificate.",
    }
    output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.inputs, args.study, args.output)
