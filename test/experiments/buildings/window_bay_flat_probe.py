"""Same source/camera86versus46 render inputs and actual profile/error/work diagrams."""

import argparse
import json
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from building_mesh import slice_triangles
from evidence import revision
from facade_entry_geometry import basis_for
from facade_window_bays import demand, flat_error
from recess_probe import building
from verify_window_bay_flat import panes, project
from verify_window_bays import camera, prepared, sources
from window_bay_geometry import products
from window_bay_probe import run as whole_probe


def diagram(current, owner, output, name):
    figure, axes = plt.subplots(1, 3, figsize=(28, 9))
    basis = basis_for(current["frame"])
    origin = np.r_[current["centre"], current["base_m"]]
    for mode, colour, label in (
        ("compact", "#376a8b", "86 · vertiefte Scheiben"),
        ("flat", "#d6841f", "46 · äußere Scheibenebenen"),
    ):
        parts = products(current, owner, 1, mode)
        points = np.concatenate([p["vertices"][p["faces"]] for p in parts])
        local = (points - origin) @ basis.T
        mesh = {
            "vertices": local.reshape(-1, 3),
            "faces": np.arange(local.size // 3).reshape(-1, 3),
            "materials": np.concatenate([p["materials"] for p in parts]),
        }
        for index, (line, _) in enumerate(slice_triangles(mesh, [0, 1, 0], 1.6)):
            axes[0].plot(
                line[:, 0],
                line[:, 2],
                color=colour,
                label=label if index == 0 else None,
            )
    axes[0].axhspan(
        -current["prototype"].back_m,
        0,
        color="grey",
        alpha=0.25,
        label="Hinter opaker Anschlusszelle",
    )
    axes[0].set(
        aspect="equal",
        title="Tatsächlicher Mittelfenster-Schnitt",
        xlabel="Fassaden-U [m]",
        ylabel="Außennormale [m]",
    )
    axes[0].legend()
    a, b = panes(current, owner, "compact"), panes(current, owner, "flat")
    counts = {
        m: sum(len(p["faces"]) for p in products(current, owner, 1, m))
        for m in ("through", "compact", "flat")
    }
    samples = []
    for distance in np.geomspace(10, 10000, 61):
        view = camera(current, float(distance))
        actual = float(
            np.linalg.norm(project(b, view) - project(a, view), axis=1).max()
        )
        mode = demand(current, view)
        samples.append(
            {
                "distance_m": float(distance),
                "observed_pane_error_px": actual,
                "bound_px": flat_error(current, view),
                "mode": mode,
                "triangles_with_cell": counts.get(mode, 0),
            }
        )
    x = [r["distance_m"] for r in samples]
    axes[1].loglog(
        x, [r["bound_px"] for r in samples], label="Konservative gerichtete Schranke"
    )
    axes[1].loglog(
        x,
        [r["observed_pane_error_px"] for r in samples],
        label="Tatsächliche Scheibenpunkte",
    )
    axes[1].axhline(0.5, color="red", linestyle="--", label="Unveränderte0,5px-Grenze")
    edge = camera(current, 300)
    edge = replace(edge, fov_deg=140, target=edge.target + basis[0] * 600)
    axes[1].scatter(
        [300],
        [flat_error(current, edge)],
        marker="x",
        s=80,
        label="140°/Bildrand:86er-Rückfall",
    )
    axes[1].set(
        title="Positionsfehler ·1920×1080", xlabel="Abstand [m]", ylabel="Pixel"
    )
    axes[1].legend()
    axes[2].step(x, [r["triangles_with_cell"] for r in samples], where="post")
    axes[2].set(
        xscale="log",
        title="Tatsächliche Dreiecke je Zelle",
        xlabel="Abstand [m]",
        ylabel="Dreiecke inklusive Anschlusszelle",
    )
    figure.tight_layout()
    figure.savefig(output / f"{name}-flat-profile.png", dpi=120)
    plt.close(figure)
    return samples


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before far-bay comparisons")
    whole_probe(output)
    comparisons, profiles = [], {}
    for index, source in enumerate(sources()):
        row = source | {
            "placement_base_enu_m": 0.0,
            "ground_asl_m": None,
            "placement_origin": "flat no DEM",
        }
        *_, owner, plan = prepared(row)
        current = plan["cells"][0]
        name = "Rect" if index == 0 else "Chamfer"
        view = camera(current, 500)
        with patch("facade_window_bays.flat_error", return_value=float("inf")):
            old = building(
                row,
                view,
                name + "86",
                output,
                proof,
                scope="Same source/camera with the prior86-triangle exterior; no geometry/image pixel oracle mutation",
            )
        new = building(
            row,
            view,
            name + "46",
            output,
            proof,
            scope="Same source/camera;46-triangle render-only surface admitted by0.5px position bound, no light/shadow/native admission",
        )
        comparisons.append(
            {"reference": old, "candidate": new, "bound_px": flat_error(current, view)}
        )
        profiles[name] = diagram(current, owner, output, name)
        if index == 0:
            edge = camera(current, 300)
            edge = replace(
                edge,
                fov_deg=140,
                target=edge.target + basis_for(current["frame"])[0] * 600,
            )
            comparisons.append(
                {
                    "fallback": building(
                        row,
                        edge,
                        "RectFallback",
                        output,
                        proof,
                        scope="Actual wide-angle0.5px rejection; previous86surface retained",
                    ),
                    "bound_px": flat_error(current, edge),
                }
            )
    (output / "flat-bay-comparison.json").write_text(
        json.dumps(
            proof
            | {
                "comparisons": comparisons,
                "profiles": profiles,
                "scope": "Restricted positional admission with opaque source-cell attachment; not a general simplification envelope, topology/shading/LOD or device admission",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
