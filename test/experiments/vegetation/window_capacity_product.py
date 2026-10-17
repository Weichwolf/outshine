"""Rebind complete cached windows to finite size classes, without new growth or needles."""

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_whole_products import load_catalog
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern
from parent_profile_graph import ParentProfileGraph
from spruce_growth_product import publish
from window_capacity_catalog import rebind
from window_portrait_source import source


def build(product, original_catalog, capacity_catalog, output):
    code = Path(__file__).resolve()
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            str(code),
            str(code.with_name("window_capacity_catalog.py")),
        ]
    ):
        raise ValueError("freeze window-capacity binding first")
    with (
        patch.object(
            ParentProfileGraph, "instance", side_effect=AssertionError("new growth")
        ),
        patch.object(
            ParentProfileGraph, "instances", side_effect=AssertionError("new growth")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("needle poses")
        ),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("whole organs")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("needle mesh")),
    ):
        _, axes, windows, _, proof = source(
            product, product / "execution", original_catalog
        )
        old, _ = load_catalog(original_catalog)
        new, _ = load_catalog(capacity_catalog)
        version = json.loads((capacity_catalog / "source.json").read_text())["version"]
        if version != "finite-window-capacities-1":
            raise ValueError("explicit supported finite size-class catalog required")
        result = rebind(windows, old, new)
        for key in windows.keys() - {"capacities", "families", "weights"}:
            np.testing.assert_array_equal(result[key], windows[key])
        output.mkdir(parents=True, exist_ok=False)
        execution = output / "execution"
        execution.mkdir()
        for name in ("axes.npz", "graph.json", "execution.json"):
            shutil.copy2(product / "execution" / name, execution / name)
        recipe = json.loads((product / "plant-recipe.json").read_text())
        np.savez_compressed(output / "windows.npz", **result)
        report = json.loads((execution / "execution.json").read_text())
        manifest = publish(output, recipe, execution, report, capacity_catalog)
        manifest["representation_version"] = "capacity-matched-windows-1"
        manifest["window_producer"] = {
            "commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "input": proof["files"],
            "implementation_sha256": {
                str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (code, code.with_name("window_capacity_catalog.py"))
            },
        }
        (output / "plant-product.json").write_text(
            json.dumps(manifest, indent=2) + "\n"
        )
        source(output, execution, capacity_catalog)
        counts = {
            str(c["needles"]): int(np.count_nonzero(result["families"] == c["family"]))
            for c in new.values()
        }
        print(
            json.dumps(
                {
                    "identity": manifest["plan_identity"],
                    "windows": len(result["origins"]),
                    "represented_needles": int(result["needle_counts"].sum()),
                    "by_capacity": counts,
                    "axis_bytes": sum(a.nbytes for a in axes.values()),
                    "window_bytes": sum(a.nbytes for a in result.values()),
                    "scope": "Same plant identity, complete old axes/needle ledger/poses; only representation family/capacity/weight changes. No new growth/needle poses/meshes.",
                }
            ),
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "original-catalog", "capacity-catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    build(args.product, args.original_catalog, args.capacity_catalog, args.output)
