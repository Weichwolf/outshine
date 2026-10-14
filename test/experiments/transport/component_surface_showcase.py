"""Close the eight real flat products; publish whole Tokyo/Central Park snapshots."""

import hashlib
import json
import shutil
import subprocess
from pathlib import Path

import numpy as np
from ground_surface_showcase import combined
from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface
from surface_solid_checks import audit_closed
from surface_solid_showcase import write_case

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "build/experiments/transport/components"
OUTPUT = BASE / "snapshots"


def solid_product(path):
    with np.load(path) as data:
        top = Mesh(
            np.c_[data["vertices"], np.zeros(len(data["vertices"]))], data["indices"]
        )
        roles = data["part_roles"][data["face_owners"]]
    solid = close_surface(top, 0.25)
    volume = audit_closed(solid)
    slots = np.full(len(solid.triangles), 2, dtype=int)
    slots[: len(top.triangles)] = np.where(roles, 1, 0)
    return solid, slots, volume


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    inventory = json.loads((BASE / "inventory.json").read_text())
    configurations, checks = [], []
    for site, _, _ in inventory["sites"]:
        result_path = BASE / "results" / site / "components/result.json"
        result = json.loads(result_path.read_text())
        if not result["report"]["complete"]:
            raise ValueError("default complete flat product required for " + site)
        models = [
            solid_product(result_path.parent / f"product-{index}.npz")
            for index in range(result["products"])
        ]
        check = {
            "site": site,
            "closed_bodies": len(models),
            "solid_triangles": sum(len(m.triangles) for m, _, _ in models),
            "volume_m3": sum(volume for _, _, volume in models),
            "expected_volume_m3": 0.25 * result["area_m2"],
        }
        if abs(check["volume_m3"] - check["expected_volume_m3"]) > max(
            1e-6, check["expected_volume_m3"] * 1e-10
        ):
            raise ValueError("independent prism volume mismatch")
        checks.append(check)
        if site not in ("Tokyo", "CentralPark"):
            continue
        mesh, slots = combined([(m, c) for m, c, _ in models])
        report = {
            "scope": "Complete captured source tile, supported flat ground axes; chosen Z=0 recipe, not DEM/functional vehicle routes.",
            "source_input_sha256": result["input_sha256"],
            "source_usage": str(
                (result_path.parent / "source-usage.json").relative_to(ROOT)
            ),
            "source_geometry_result_sha256": hashlib.sha256(
                result_path.read_bytes()
            ).hexdigest(),
            "height_assumption_m": 0.0,
            "depth_m": 0.25,
            "valid_surface": True,
            "valid": True,
        }
        write_case(site, mesh, slots, report, configurations, output_dir=OUTPUT)
        destination = (
            ROOT / "build/experiments/transport/inputs" / (site + "-input.json")
        )
        source = BASE / "inputs" / destination.name
        if destination.exists() and destination.read_bytes() != source.read_bytes():
            raise ValueError("refusing to replace an existing source input")
        shutil.copy2(source, destination)
    records = {
        Path(c["output"]).stem: {
            "report_sha256": hashlib.sha256(
                Path(c["output"]).with_suffix(".json").read_bytes()
            ).hexdigest()
        }
        for c in configurations
    }
    origin = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)
        ),
        "records": records,
    }
    (OUTPUT / "origin.json").write_text(json.dumps(origin, indent=2) + "\n")
    (BASE / "solid-checks.json").write_text(json.dumps(checks, indent=2) + "\n")
    print(
        json.dumps(
            {"closed_source_tiles": len(checks), "snapshots": len(configurations)}
        )
    )


if __name__ == "__main__":
    main()
