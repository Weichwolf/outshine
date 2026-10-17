"""Inspect original OBJ UVs and source-owned material resources."""

from dataclasses import asdict
import json
import time

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from modelling_asset_cases import fixtures
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from outshine_ref.buildings.modelling.shape import Shape
from modelling_scope_probe import engine_mesh
from showcase_groups import COLOURS, save_plot
from showcase_profile import PROFILE
from showcase_scene import fitted_points
from showcase_split import render_control


def original(asset):
    return Shape(
        asset.geometry,
        Scope(position=asset.minimum, size=asset.geometry.reference_size),
        asset=asset,
    )


def uv_plan(output, provenance, asset):
    start = time.perf_counter()
    fig, ax = plt.subplots(figsize=(12, 8))
    mesh = original(asset).emit()
    for group in np.unique(mesh["groups"]):
        for row in mesh["faces"][mesh["groups"] == group]:
            ax.plot(
                *np.r_[mesh["uv"][row], mesh["uv"][row[:1]]].T,
                color=COLOURS[int(group) % 4],
                linewidth=1.5,
            )
    ax.set(
        title="OBJ · UV0\n" + asset.import_info.uv_convention,
        aspect="equal",
        xlim=(0, 1),
        ylim=(0, 1),
        xlabel="u",
        ylabel="v",
    )
    ax.grid(alpha=0.2)
    title = "Dateiimport · ursprüngliche OBJ-Texturkoordinaten"
    fig.suptitle(title)
    fig.tight_layout()
    return save_plot(
        fig,
        output,
        "current-import-uv-plan",
        title,
        "Eigene OBJ-Fläche mit ursprünglichen UV0-Koordinaten; Textur-Shading separat.",
        provenance,
        start,
        [1920, 1080],
        label="modelling-import",
    )


def render_import(output, provenance, asset):
    children = original(asset).comp("g")
    points = np.concatenate(
        [engine_mesh(child.emit())["vertices"] for child in children]
    )
    camera = fitted_points(points, PROFILE, direction=(0.3, -1.0, 0.2))
    name = render_control(
        output,
        provenance,
        "import-obj",
        "OBJ · negative Indices, Gruppen und Quellmaterial",
        children,
        camera,
    )
    path = output / name
    record = json.loads(path.read_text())
    record.update(
        label="modelling-import",
        caption="Eigene OBJ-Datei: Quellmaterialwerte, UV0/Normalen und Gruppen erhalten. Opake Prüfpalette; Textur-/PBR-Shading separat.",
        geometry_scope="complete supported surface nodes of an owned OBJ asset",
        import_source=asdict(asset.import_info),
        material_parameters=[
            dict(id=i, name=v.name, values=dict(v.attributes))
            for i, v in asset.geometry.material_defs
        ],
    )
    path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    return name


def probe(output, provenance):
    asset = Asset.from_file(fixtures(output / "owned-import-files"))
    return [
        uv_plan(output, provenance, asset),
        render_import(output, provenance, asset),
    ]
