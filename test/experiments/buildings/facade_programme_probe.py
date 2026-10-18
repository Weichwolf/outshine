"""Actual source comparisons of complete programmes, with the legacy camera poses."""

import argparse
import hashlib
import json
from dataclasses import asdict
from pathlib import Path

from bay_city_checks import inspect as inspect_layers
from bay_facade_plan import plan_bays
from bay_facade_probe import near
from bay_facade_probe import palette as previous_palette
from bay_wall_domains import domains
from evidence import revision
from facade_programme_plan import PROGRAMMES, plan_programme
from facade_programme_products import ProgrammeProducts, palette
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_facade_prepare import positions
from terraced_mass_admission import inspect
from terraced_mass_sources import sources
from window_glass_probe import views


def diagram(output, name, terrace, programmes):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from building_plan_views import patch

    walls, _rejected = domains(
        terrace.prepared, portals=terrace.portals, outward_m=0.18, include_blank=True
    )
    common = set.intersection(*({c.plane_id for c in p.cells} for p in programmes))
    wall = max(
        (w for w in walls if w.identifier in common and len(w.windows) >= 4),
        key=lambda w: len(w.windows),
    )
    origin = positions(np.array([[*wall.origin_xy, 0.0]]), terrace.prepared.mass.core)[
        0, :2
    ]
    core = terrace.prepared.mass.core
    tangent = wall.tangent[0] * np.asarray(core.across_xy) + wall.tangent[
        1
    ] * np.asarray(core.along_xy)
    colors = previous_palette() | palette()
    import shapely
    from shapely.affinity import translate

    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    for axis, programme in zip(axes, programmes, strict=True):
        patch(
            axis,
            shapely.from_wkb(wall.region_wkb),
            facecolor="#dad8d0",
            edgecolor="#666666",
            linewidth=0.3,
        )
        for cell in programme.cells:
            if cell.plane_id != wall.identifier:
                continue
            for layer in cell.layers:
                region = translate(
                    layer.region,
                    xoff=float((layer.origin_xy - origin) @ tangent),
                    yoff=layer.base_m,
                )
                definition = colors[str(layer.material)]
                color = tuple(definition["color." + key] for key in ("r", "g", "b"))
                patch(
                    axis, region, facecolor=color, edgecolor="#444444", linewidth=0.25
                )
        axis.set(
            aspect="equal",
            xlabel="Actual wall U (m)",
            ylabel="Source body height (m)",
            title=programme.programme,
        )
        axis.autoscale_view()
    fig.suptitle(
        wall.identifier + " · unchanged openings; plinth/main/attic and field axes"
    )
    fig.tight_layout()
    fig.savefig(output / (name + "-elevation.png"), dpi=100)
    plt.close(fig)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the programme reference before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for index, kind in ((0, "cascade"), (3, "paired-wings")):
        row = sources()[index]
        source = RoofedProducts(plan(row, kind))
        legacy = source.draw(True)
        terrace = source.body.plan
        programmes = [
            plan_programme(terrace.prepared, style, portals=terrace.portals)
            for style in PROGRAMMES
        ]
        prefix = f"Programme{index}-{kind}"
        diagram(output, prefix, terrace, programmes)
        window, poses = views(source, legacy)
        old_bays = plan_bays(terrace.prepared, "framed-bays", portals=terrace.portals)
        _cell, poses["Near"] = near(old_bays, window)
        poses = {key: poses[key] for key in ("Near", "Whole")}
        poses["Opposite"] = camera(overview(legacy["vertices"], [-1, 1.25, 0.65]))
        for label, planned in [
            ("Legacy", None),
            *zip(PROGRAMMES, programmes, strict=True),
        ]:
            name = prefix + "-" + label
            product = None if planned is None else ProgrammeProducts(source, planned)
            mesh = legacy if product is None else product.draw()
            checks = (
                None
                if product is None
                else {
                    "body": inspect(product.body),
                    "added_layers": inspect_layers(planned, terrace),
                }
            )
            plan_path = output / (name + "-plan.json")
            plan_path.write_text(
                json.dumps(
                    None if planned is None else asdict(planned),
                    default=lambda v: v.hex(),
                    indent=2,
                )
                + "\n"
            )
            record = {
                "source_id": row["id"],
                "source_properties": row["properties"],
                "source_footprint_wkb": row["polygon"].wkb_hex,
                "camera": poses["Whole"],
                "source": {
                    "material_definitions": previous_palette()
                    | ({} if planned is None else palette())
                },
                "facade_materials": {},
                "ground_span_m": 10000,
                "programme_plan": str(plan_path),
                "programme_plan_sha256": hashlib.sha256(
                    plan_path.read_bytes()
                ).hexdigest(),
                "physical_checks": checks,
                "legacy_triangles": len(legacy["faces"]),
                "legacy_trim_faces": int(
                    (source.body.facade.physical().frames["materials"] == 9).sum()
                ),
                "explicit_reference_old_trim_built_then_replaced": planned is not None,
                "scope": "Actual source body, heights, windows/portals, floors, terraces and roof retained. Only owned legacy trim is replaced by plinth/main/attic and horizontal/vertical facade programmes. Reference deliberately builds then removes old trim; early construction and whole-city/runtime/cache/native admission pending.",
            }
            write(output, name, mesh, record, proof)
            saved = json.loads((output / (name + "-geometry.json")).read_text())
            for pose, value in poses.items():
                alias = name + pose
                path = output / (alias + "-geometry.json")
                path.write_text(
                    json.dumps(saved | {"city": alias, "camera": value}, indent=2)
                    + "\n"
                )
                records.append(
                    {"name": alias, "geometry": str(path), "source_id": row["id"]}
                )
    (output / "models.json").write_text(
        json.dumps(proof | {"records": records}, indent=2) + "\n"
    )
    print(json.dumps({"models": len(records), "output": str(output)}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit directory under /tmp")
    models(output)


if __name__ == "__main__":
    main()
