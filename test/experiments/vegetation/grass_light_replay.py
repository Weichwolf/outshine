"""Fresh installed reference: same cached geometry, shared index, changed light state."""

import argparse
import copy
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
from grass_light_model import lighting
from grass_projection_reader import read_product
from grass_shadow import GrassShadowIndex


def replay(inputs, output):
    receipt = json.loads((inputs / "result.json").read_text())
    source = Path(receipt["source_inputs"])
    if (
        hashlib.sha256((source / "result.json").read_bytes()).hexdigest()
        != receipt["source_report_sha256"]
    ):
        raise ValueError("original saved contact source changed")
    recipe = json.loads((source / "stand.json").read_text())["recipe"]
    with (
        patch(
            "outshine_ref.vegetation.ground_cover.GroundCoverPlan.sites",
            side_effect=AssertionError("placed roots"),
        ),
        patch(
            "outshine_ref.vegetation.graminoid.graminoid_tuft_mesh",
            side_effect=AssertionError("meshed blades"),
        ),
        patch(
            "grass_projection.contacts", side_effect=AssertionError("rebuilt contacts")
        ),
        patch(
            "grass_projection_instances.instanced_contacts",
            side_effect=AssertionError("expanded contacts"),
        ),
    ):
        report, stand, products = read_product(source)
        index = GrassShadowIndex(stand["vertices"], stand["faces"])
        view = np.asarray(receipt["view"])
        product = products[receipt["scenario"]["view"]]
        results = {}
        with patch(
            "grass_shadow.GrassShadowIndex.__init__",
            side_effect=AssertionError("rebuilt index for light"),
        ):
            for name, state in receipt["states"].items():
                result = lighting(
                    index,
                    recipe,
                    product,
                    np.asarray(report["footprint_m"]),
                    view,
                    state,
                    receipt["scenario"]["bias_m"],
                )
                with np.load(inputs / f"{name}.npz", allow_pickle=False) as expected:
                    for key in expected.files:
                        np.testing.assert_array_equal(result[key], expected[key])
                results[name] = result
            doubled = copy.deepcopy(receipt["states"]["back"])
            for lamp in doubled["lights"]:
                lamp["irradiance_rgb"] = (
                    2 * np.asarray(lamp["irradiance_rgb"])
                ).tolist()
            brighter = lighting(
                index,
                recipe,
                product,
                np.asarray(report["footprint_m"]),
                view,
                doubled,
                receipt["scenario"]["bias_m"],
            )
            np.testing.assert_array_equal(
                brighter["radiance"], 2 * results["back"]["radiance"]
            )
    result = {
        "stand_identity": report["stand_identity"],
        "frames_exact": list(results),
        "generation_forbidden": True,
        "shared_geometry_index": True,
        "light_change_linear": True,
        "scope": "Ephemeral direct-light frames; never cached world illumination. No native/GPU/canopy-transport acceptance",
    }
    output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    replay(args.inputs, args.output)
