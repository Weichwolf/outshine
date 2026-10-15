"""Private actual-source 2.5D, opposite, terrace and night controls; no public page expansion."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from showcase_tiles import frame_proof
from stepped_openings import apply as opening_patches
from stepped_source import exterior, near, plan_for
from tile_buildings import read_tile


def overview(points, direction):
    target = (points.min(axis=0) + points.max(axis=0)) / 2
    direction = np.asarray(direction, dtype=float)
    direction /= np.linalg.norm(direction)
    view = View(target + direction, target, width=1920, height=1080, fov_deg=48.0)
    q = (points - target) @ view.basis.T
    required = (
        np.maximum(
            abs(q[:, 0]) * view.focal_px / (view.width * 0.44),
            abs(q[:, 1]) * view.focal_px / (view.height * 0.40),
        )
        - q[:, 2]
    )
    return View(
        target + direction * (required.max() + 1.0),
        target,
        width=1920,
        height=1080,
        fov_deg=48.0,
    )


def save_products(output, detail, outer):
    products = {}
    for name, product in (("near", detail), ("far", outer)):
        path = output / ("stepped-source-" + name + "-mesh.npz")
        np.savez_compressed(path, **product)
        products[name] = (path, hashlib.sha256(path.read_bytes()).hexdigest())
    return products


def probe(output, site, identifier, spp, prepare_only=False):
    output.mkdir(parents=True, exist_ok=True)
    rows, receipt = read_tile(site)
    row = next(r for r in rows if r["id"] == identifier)
    plan = plan_for(row)
    detail, access, terraces, light = near(plan, 1)
    outer, envelope, guard, comparison = exterior(plan, 1)
    outer, opening_records = opening_patches(plan, outer, 32768)
    provenance = revision()
    products = save_products(output, detail, outer)
    reports = []
    primary = overview(detail["vertices"], [1.0, -1.25, 0.80])
    opposite = overview(detail["vertices"], [-1.0, 1.25, 0.80])
    record = plan["terrace_access"][0]
    current = plan["storey_plans"][record["storey"]]
    door = next(
        d
        for d in current["doors"]
        if d["kind"] == "terrace" and d["id"] == "terrace-" + str(record["component"])
    )
    tangent = np.asarray(door["tangent"])
    normal = np.array([tangent[1], -tangent[0]])
    centre = np.asarray(door["centre"])
    close = View(
        [*(centre + normal * 10.0 + tangent * 0.7), current["floor_z_m"] + 1.75],
        [*(centre - normal * 0.4), current["floor_z_m"] + 1.1],
        width=1920,
        height=1080,
        fov_deg=48.0,
    )
    for name, kind, view, night in (
        ("2_5d", "near", primary, False),
        ("opposite", "near", opposite, False),
        ("far", "far", primary, False),
        ("terrace", "near", close, False),
        ("night", "near", close, True),
    ):
        path, digest = products[kind]
        stem = "stepped-source-" + name
        image = output / (stem + ".png")
        config = dict(
            profile=[1920, 1080],
            mesh=str(path.resolve()),
            mesh_sha256=digest,
            eye=np.asarray(view.eye).tolist(),
            target=np.asarray(view.target).tolist(),
            fov_deg=view.fov_deg,
            wall_linear=[0.68, 0.67, 0.64],
            roof_linear=[0.22, 0.24, 0.25],
            ground_span_m=receipt["span_m"] * 1.5,
            spp=spp,
            seed=17,
            denoise=True,
            night=night,
            room_lights=light if night else [],
            output=str(image.resolve()),
        )
        if kind == "far":
            config["facade_materials"] = {
                "32768": dict(wall_material=1, fractions=[1.0, 1.0], filtered=True)
            }
        scene = output / (stem + "-scene.json")
        scene.write_text(json.dumps(config, indent=2) + "\n")
        evidence = provenance | dict(
            id=identifier,
            source_properties=row["properties"],
            receipt=receipt["receipt"],
            view=name,
            representation=kind,
            profile=[1920, 1080],
            form_recipe=plan["form_recipe"],
            access=access,
            terraces=terraces,
            opening_records=opening_records,
            exterior_method=comparison,
            source_room_ids=plan["storey_room_ids"],
            night_fixture_ids=[r["room_id"] for r in light],
            scope="Actual generic stepped source; room programme, full LOD/cache, native controller/frame/device remain unproved",
        )
        if name in ("2_5d", "opposite"):
            evidence["frame"] = frame_proof(detail, view)
        if not prepare_only:
            log = Path(tempfile.gettempdir()) / (
                "outshine-buildings-" + stem + "-" + provenance["commit"][:9] + ".log"
            )
            with log.open("w") as stream:
                subprocess.run(
                    [
                        sys.executable,
                        str(Path(__file__).with_name("mitsuba_interior.py")),
                        str(scene),
                    ],
                    stdout=stream,
                    stderr=subprocess.STDOUT,
                    check=True,
                    timeout=900,
                )
            renderer = json.loads(image.with_suffix(".renderer.json").read_text())
            if renderer["included_source_triangles"] != len(
                (detail if kind == "near" else outer)["faces"]
            ):
                raise ValueError(
                    "Private stepped render omitted actual source triangles"
                )
            evidence.update(
                renderer=renderer,
                image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
            )
        report = output / (stem + "-report.json")
        report.write_text(json.dumps(evidence, indent=2) + "\n")
        reports.append(report.name)
        print(name, "prepared" if prepare_only else "rendered", flush=True)
    (output / "reports.json").write_text(
        json.dumps(
            provenance | dict(reports=reports, prepared_only=prepare_only), indent=2
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/stepped-controls"),
    )
    parser.add_argument("--site", default="Hameln")
    parser.add_argument("--id", default="Hameln/14/8617/5404/building/1267876880/11/0")
    parser.add_argument("--spp", type=int, default=32)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    probe(args.output, args.site, args.id, args.spp, args.prepare_only)
