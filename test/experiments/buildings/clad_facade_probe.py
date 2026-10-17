"""Actual broad facade regions, paired building views and the complete current city."""

import argparse
import json
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_plan_views import patch as draw_polygon
from city_scene import inputs
from clad_facade_mesh import CladFacades, CladFrames, build, surface
from clad_facade_plan import placement
from clad_facade_sources import sources
from evidence import revision
from facade_frame_geometry import placed
from facade_frame_probe import views
from industrial_relief_probe import camera
from podium_probe import write
from showcase_tiles import view_for as city_view
from tile_buildings import construct
from tile_podiums import PodiumDetails
from verify_facade_frames import build as original_build
from verify_facade_frames import view_for


def diagrams(output, row, info, owner, index):
    f = next(
        f
        for f in info["facade_plan"]
        if f["id"] == owner.frames.completed[0]["facade_id"]
    )
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    for axis, kind in zip(axes, ("broad-piers", "broad-spandrels"), strict=True):
        p = placement(f, 0, info["facade_recipe"], kind)
        draw_polygon(axis, p.region, facecolor="#4b6571", edgecolor="none")
        draw_polygon(
            axis, p.protected, facecolor="#e9c997", alpha=0.5, edgecolor="none"
        )
        axis.set(
            aspect="equal",
            title=kind,
            xlabel="Fassaden-U [m]",
            ylabel="Höhe ab Programm-Basis [m]",
        )
        axis.autoscale_view()
    fig.suptitle(
        row["id"] + " · tatsächliche Bekleidung und geschützte Fenster/Einfassungen"
    )
    fig.tight_layout()
    fig.savefig(output / f"{index}-clad-sections.png", dpi=100)
    plt.close(fig)


def export(output, name, mesh, info, view, owner, proof):
    write(
        output,
        name,
        mesh,
        {
            "camera": camera(view),
            "source": {
                "material_definitions": {
                    "12": {
                        "color.r": 0.72,
                        "color.g": 0.71,
                        "color.b": 0.68,
                        "roughness": 0.78,
                        "metallic": 0.0,
                    },
                }
            },
            "facade_materials": owner.bindings,
            "source_info": info,
            "ground_span_m": 10000.0,
            "scope": "Actual source geometry and unchanged source window/entry/projection fields; optional composed facade skin. Existing frame metal material, no new shader contract or native admission.",
        },
        proof,
    )


def forced(method):
    def product(self, current, resolved, material):
        if method == "omit":
            return None, "deliberately-omitted"
        mode = "outer" if method == "full" else "front"
        return placed(surface(current, mode), current, 10), mode

    return product


def models(output, case=None):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze clad programme before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index, (row, neighbors) in enumerate(sources()):
        if case is not None and index != case:
            continue
        baseline, info, _old = original_build(row, neighbors, view_for(row))
        after, changed, owner = build(row, neighbors, view_for(row))
        diagrams(output, row, changed, owner, index)
        _record, cameras = views(
            np.concatenate([baseline["vertices"], after["vertices"]]),
            info,
            owner.frames.completed,
        )
        for label, view in cameras:
            if label == "Far":
                continue
            for kind, operation in (("Before", original_build), ("After", build)):
                mesh, data, builder = operation(row, neighbors, view)
                export(
                    output, f"{index}{kind}{label}", mesh, data, view, builder, proof
                )
        # Three equal full-building far choices use the same finite distance.
        view = cameras[0][1]
        view = replace(view, eye=view.target + (view.eye - view.target) * 6)
        for method in ("full", "front", "omit"):
            with patch.object(CladFrames, "product", forced(method)):
                mesh, data, builder = build(row, neighbors, view)
            export(output, f"{index}Far{method}", mesh, data, view, builder, proof)


def near_controls(output, case=None):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze near counterexample source")
    output.mkdir(parents=True, exist_ok=True)
    for index, (row, neighbors) in enumerate(sources()):
        if case is not None and index != case:
            continue
        mesh, info, owner = build(row, neighbors, view_for(row))
        _record, cameras = views(mesh["vertices"], info, owner.frames.completed)
        view = next(v for label, v in cameras if label == "Near")
        for method in ("full", "front"):
            with patch.object(CladFrames, "product", forced(method)):
                mesh, data, builder = build(row, neighbors, view)
            export(
                output, f"{index}NearControl{method}", mesh, data, view, builder, proof
            )


def city_models(census, output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze clad city source before evidence")
    rows, receipts, window = inputs("Paris", json.loads(census.read_text()))
    view = replace(city_view(rows), width=1920, height=1080)
    output.mkdir(parents=True, exist_ok=True)

    def progress(requested, accepted, identifier):
        print(
            json.dumps(
                {"requested": requested, "accepted": accepted, "id": identifier}
            ),
            flush=True,
        )

    owner = PodiumDetails(rows, view, progress=progress)
    for after in (False, True):
        if after:
            owner.facades = CladFacades(owner.rows, view)
        owner.completed.clear()
        owner.rejected.clear()
        mesh, source = construct(
            "Paris",
            prepared=(owner.rows, {"invalid": [], "span_m": window["width_m"]}),
            ordinary_builder=owner,
        )
        if source["generated"] != window["building_polygons"] or source["omissions"]:
            raise ValueError("Clad city lost original sources")
        write(
            output,
            "ParisClad" + ("After" if after else "Before"),
            mesh,
            {
                "camera": camera(view),
                "source": source,
                "sources": receipts,
                "window": window,
                "facade_materials": owner.facades.bindings,
                "procedures": owner.report(),
                "clad_completed": owner.facades.frames.completed,
                "clad_rejected": owner.facades.frames.rejected,
                "scope": "Complete current988-building source window, all eight admitted podiums retained, flat without DEM; same prepared podium assets in both passes. No cache/native/frame admission.",
            },
            proof,
        )
        print(
            json.dumps(
                {
                    "after": after,
                    "generated": source["generated"],
                    "triangles": len(mesh["faces"]),
                    "frames": len(owner.facades.frames.completed),
                }
            ),
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--census", type=Path)
    parser.add_argument("--negative-near", action="store_true")
    parser.add_argument("--case", type=int, choices=range(len(sources())))
    args = parser.parse_args()
    if args.negative_near:
        near_controls(args.output, args.case)
    elif args.census is None:
        models(args.output, args.case)
    else:
        city_models(args.census, args.output)
