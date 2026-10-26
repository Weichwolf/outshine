"""Compare stored compact output with an immutable legacy consumer and known failures."""

import argparse
import hashlib
import importlib.util
import itertools
import json
import sys
from pathlib import Path

import numpy as np


def legacy_module(path):
    name = "prism_legacy_" + hashlib.sha256(path.read_bytes()).hexdigest()[:12]
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def run(args):
    from attributed_prisms import load_product, native_inputs, render_product
    from compare_attributed_prisms import check_view
    from compare_plane_cloud import image_roi
    from full_view_reference import load, views
    from prepare_prism_lighting import camera_cases
    from triangle_image_reference import image_rays, prepare_source

    args.output.mkdir(parents=True, exist_ok=False)
    old = legacy_module(args.legacy)
    legacy = old.load_product(args.product / "dense-product.npz")
    current = load_product(args.product / "product.npz")
    source, document, context, _ = load(args.input)
    poses = views(document, context)
    extra, _, _ = camera_cases(source, document, context)
    poses.update({name: view for name, view in extra.items() if name not in poses})
    corners = np.array(
        [
            list(itertools.product(*zip(box[0], box[1], strict=True)))
            for box in legacy[0].bounds
        ]
    ).reshape(-1, 3)
    extent = {"positions": np.concatenate((source["positions"], corners))}
    rows = []
    for name, view in poses.items():
        box = image_roi(extent, view)
        if box is None:
            rows.append({"pose": name, "offscreen": True})
            continue
        width, height = (box[1] - box[0]).astype(int)
        origins, directions = image_rays(view, box, width, height, 2)
        before = old.render_product(legacy, origins, directions)
        after = render_product(current, origins, directions)
        for index in [0, 2, 3, 4, 5, 6]:
            np.testing.assert_array_equal(
                before[index], after[index], err_msg=f"{name}: component {index}"
            )
        for key in before[1]:
            np.testing.assert_array_equal(
                before[1][key], after[1][key], err_msg=f"{name}: {key}"
            )
        rows.append({"pose": name, "samples": len(origins), "all_results_equal": True})
        print(name, len(origins), "equal", flush=True)
    reference = prepare_source(source)
    inputs = native_inputs(source)
    counter = {
        name: check_view(
            source,
            reference,
            inputs,
            current,
            poses[name],
            extent,
            args.output / (name + ".npz"),
        )
        for name in ["moving-023", "static-025"]
    }
    report = {
        "legacy_source_sha256": hashlib.sha256(args.legacy.read_bytes()).hexdigest(),
        "poses": rows,
        "known_native_counterexamples": counter,
        "scope": "Same exact old consumer outputs, not native equivalence. Native material/coverage counterexamples remain reported, no tolerance relaxed.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--legacy", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
