"""Metric facade field plans and a paired physical 3D surface control."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from evidence import revision
from outshine_ref.buildings.facade_finish import CHANNELS, FacadeFinish
from outshine_ref.buildings.facades import FacadeRepeat
from place_materials import default_finish
from provider_contract import attributes
from tile_buildings import prism
from tile_facades import project


def plan(output):
    repeat, finish = FacadeRepeat(20.0, 15.0, 6, 5, 0.48, 0.53), FacadeFinish()
    xs, ys = (np.arange(640) + 0.5) * 20 / 640, (np.arange(480) + 0.5) * 15 / 480
    xx, yy = np.meshgrid(xs, ys)
    weights = finish.coverage(repeat, np.stack((xx, yy), axis=-1), [20 / 640, 15 / 480])
    fig, axes = plt.subplots(2, 2, figsize=(19.2, 10.8), dpi=100)
    for index, axis in enumerate(axes.flat):
        axis.imshow(
            weights[..., index],
            origin="lower",
            extent=[0, 20, 0, 15],
            vmin=0,
            vmax=1,
            cmap="viridis",
        )
        axis.set(
            title=CHANNELS[index] + " area fraction",
            xlabel="facade width [m]",
            ylabel="facade height [m]",
        )
    fig.suptitle("Same finite facade / metric frame and storey-band surface fields")
    fig.tight_layout()
    fig.savefig(output / "facade-finish-plan.png")
    plt.close(fig)


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Facade finish control requires a clean frozen source")
    output.mkdir(parents=True, exist_ok=True)
    plan(output)
    row = {
        "id": "surface-control",
        "polygon": shapely.box(0, 0, 20, 10),
        "properties": {"render_height": 15.0},
    }
    parameters = attributes(row["properties"])
    original, _ = prism(row, parameters, 12)
    mesh, selected, _ = project(original, row, parameters, (0.48, 0.53), 3.2, 3.0)
    mesh["materials"][selected] = 32768
    path = output / "facade-finish-mesh.npz"
    np.savez_compressed(path, **mesh)
    config = {
        "profile": [1920, 1080],
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "eye": [-25, -35, 25],
        "target": [10, 5, 7.5],
        "fov_deg": 35.0,
        "wall_linear": [0.68, 0.67, 0.64],
        "roof_linear": [0.22, 0.24, 0.25],
        "facade_materials": {
            "32768": {"fractions": [0.48, 0.53], "wall_material": 12, "filtered": True}
        },
        "spp": 32,
        "seed": 17,
        "denoise": True,
    }
    reports = {}
    for name in ("baseline", "finished", "finished-reference"):
        current = config | {"output": str((output / (name + ".png")).resolve())}
        if name != "baseline":
            current["facade_finishes"] = default_finish()
            if name == "finished-reference":
                current["facade_finishes"]["explicit_band_reference"] = True
        scene = output / (name + "-scene.json")
        scene.write_text(json.dumps(current, indent=2) + "\n")
        with (output / (name + ".log")).open("w") as stream:
            subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).with_name("mitsuba_interior.py")),
                    str(scene),
                ],
                stdout=stream,
                stderr=subprocess.STDOUT,
                timeout=240,
                check=True,
            )
        reports[name] = json.loads((output / (name + ".renderer.json")).read_text())
    if any(
        reports["baseline"]["projection"] != report["projection"]
        or reports["baseline"]["mesh_sha256"] != report["mesh_sha256"]
        for report in reports.values()
    ):
        raise ValueError("Surface control changed geometry or camera")
    (output / "verification.json").write_text(
        json.dumps(
            proof
            | {
                "reports": reports,
                "finish": default_finish(),
                "scope": "Planar surface/material control; no physical window recess or functional room programme",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
