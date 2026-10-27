"""Compare geographic rays with a proposed cap of the same analytic ground fit.

Cap XYZ is evaluated on the existing XY domain. It is not an already published
mesh/lease or an admitted contact. The original geographic port remains distinct.
"""

import argparse
import json
from pathlib import Path

import numpy as np
from canonical_port_native_inputs import (
    FIXTURE,
    check_file,
    feature_spans,
    load_source,
    terrain_input,
)
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import frame, positions_in_frame
from outshine_ref.transport.canonical_ports._ground import fit_ground, prepare_ground
from outshine_ref.transport.canonical_ports._queries import canonical_port
from outshine_ref.transport.canonical_ports._source import (
    component,
    source_chain,
    source_index,
)


def closed_inputs(source, budget):
    index, reason = source_index(source, budget)
    if reason:
        raise ValueError(reason)
    closed = []
    for feature, ring in ((1047395490, 4), (1604876210, 11)):
        spans = feature_spans(source, feature, ring)
        selected, reason, _work = component(
            index, spans[0].identifier, set(source.unknown_nodes), budget
        )
        if reason or set(selected) != {span.identifier for span in spans}:
            raise ValueError("fixture component is not exhausted: " + reason)
        chain, reason = source_chain(index, selected)
        if reason:
            raise ValueError(reason)
        closed.append((feature, ring, chain))
    return index, closed


def cap_row(index, feature, ring, chain, anchor, inputs, query, fit, side, terrain):
    node = chain[1][0 if side == 0 else -1]
    geo = index[0][node]
    span = index[1][index[2][node][0]]
    segment_geo = np.array([index[0][value] for value in span.nodes])
    initial = float(terrain(np.array([geo]))[0])
    try:
        numeric = canonical_port(
            query, anchor, geo, segment_geo, span.width_m / 2, initial
        )
        ray = {"state": "numeric_ground_sample", "height_m": numeric["height_m"]}
    except ValueError as error:
        ray = {"state": "Pending", "reason": str(error)}
    cap = inputs.surface.vertices[:2] if side == 0 else inputs.surface.vertices[-2:]
    xyz, gradients = [], []
    before = query.queries, query.face_bounds_tests
    for fraction in (0.0, 0.25, 0.5, 0.75, 1.0):
        xy = (1 - fraction) * cap[0, :2] + fraction * cap[1, :2]
        height, gradient = query.evaluate(xy)
        xyz.append([*xy, height])
        gradients.append(gradient)
    work = {
        "field_queries": query.queries - before[0],
        "face_bounds_tests": query.face_bounds_tests - before[1],
    }
    xyz, gradients = np.array(xyz), np.array(gradients)
    origin, source_axes = frame(anchor)
    target, target_axes = frame(geo)
    port_xyz = (origin - target + xyz @ source_axes.T) @ target_axes
    normal = target_axes.T @ source_axes @ np.r_[-gradients[2], 1.0]
    normal /= np.linalg.norm(normal)
    original = positions_in_frame(geo, segment_geo, np.zeros(2))
    direction = original[1, :2] - original[0, :2]
    direction /= np.linalg.norm(direction)
    left = np.array([-direction[1], direction[0]])
    gradient = -normal[:2] / normal[2]
    left_right = (
        [0, 4] if np.dot(port_xyz[0, :2] - port_xyz[2, :2], left) > 0 else [4, 0]
    )
    prior_line = cap[0, 2] + np.arange(5) / 4 * (cap[1, 2] - cap[0, 2])
    return {
        "feature": feature,
        "ring": ring,
        "domain_node": node,
        "edge": span.identifier,
        "component_end": side,
        "physical_end": span.nodes.index(node),
        "source_way": span.attributes["native_path"]["LocalWayBinding"]["StreetWay"],
        "source_step": span.attributes["native_source_reference"]["SegmentOrdinal"],
        "source_forward_direction_en": direction.tolist(),
        "source_frame_anchor": list(anchor),
        "geographic_source_anchor": list(geo),
        "fit_method": fit["selected_method"],
        "maximum_original_prior_error_m": fit["maximum_original_prior_error_m"],
        "geographic_ray": ray,
        "geometric_cap": {
            "source_xy": cap[:, :2].tolist(),
            "prior_source_xyz_m": cap.tolist(),
            "fit_height_change_from_prior_chord_m": (xyz[:, 2] - prior_line).tolist(),
            "port_xyz_m": port_xyz.tolist(),
            "normal_enu": normal.tolist(),
            "center_enu_m": port_xyz[2].tolist(),
            "physical_forward_left_right_enu_m": port_xyz[left_right].tolist(),
            "physical_forward_left_right_stencil_indices": left_right,
            "horizontal_offset_from_original_geographic_ray_m": float(
                np.linalg.norm(port_xyz[2, :2])
            ),
            "horizontal_cap_width_m": float(
                np.linalg.norm(port_xyz[-1, :2] - port_xyz[0, :2])
            ),
            "original_profile_width_m": span.width_m,
            "maximum_cap_gradient_component_variation": float(
                np.max(abs(gradients - gradients[2]))
            ),
            "maximum_cap_plane_residual_m": float(
                np.max(abs((port_xyz - port_xyz[2]) @ normal))
            ),
            "terminal_plane_role": 1,
            "work": work,
        },
        "cap_profile_check": {
            "longitudinal_grade": float(gradient @ direction),
            "cross_grade": float(gradient @ left),
            "maximum_gradient": span.maximum_gradient,
            "passes_existing_central_port_grade_checks": bool(
                abs(gradient @ direction) <= span.maximum_gradient + 1e-9
                and abs(gradient @ left) <= 0.025 + 1e-9
            ),
        },
    }


def compare(output):
    source, manifest = load_source()
    budget = PortBudget()
    index, closed = closed_inputs(source, budget)
    check_file(FIXTURE, manifest, "dem.webp")
    sampler, binding = terrain_input(FIXTURE, manifest)
    calls = []

    def terrain(at):
        calls.append(len(at))
        return sampler(at)

    rows = []
    for feature, ring, chain in closed:
        anchor, inputs, _samples = prepare_ground(chain, terrain, budget)
        query, fit = fit_ground(inputs, 0.5, "auto", budget)
        if query is None or inputs.part_roles[0] != 1 or inputs.part_roles[-1] != 1:
            raise ValueError(
                "a qualified fixture field with affine end bands is required"
            )
        for side in (0, 1):
            rows.append(
                cap_row(
                    index,
                    feature,
                    ring,
                    chain,
                    anchor,
                    inputs,
                    query,
                    fit,
                    side,
                    terrain,
                )
            )
    report = {
        "scope": "Proposed XYZ cap of the same analytic ground fit and bound XY domain; no published mesh/lease, geometric-contact agreement, candidate correspondence, bridge/clearance/route/native admission. Original JSON-double recipes, 0.5m prior and all old tolerances retained.",
        "source_raw_sha256": manifest["native_raw_sha256"],
        "recipe_source_sha256": manifest["recipe_source_sha256"],
        "terrain_binding": binding,
        "rows": rows,
        "DEM_calls": len(calls),
        "DEM_samples": sum(calls),
        "old_geographic_ray_still_pending": sum(
            row["geographic_ray"]["state"] == "Pending" for row in rows
        ),
        "cap_work": {
            key: sum(row["geometric_cap"]["work"][key] for row in rows)
            for key in ("field_queries", "face_bounds_tests")
        },
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "ground-cap-contact-proposal.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    return report


def plot(report, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 2, figsize=(12, 7.5), layout="constrained")
    for ax, row in zip(axes.flat, report["rows"], strict=True):
        cap = np.array(row["geometric_cap"]["port_xyz_m"])
        across = cap[-1, :2] - cap[0, :2]
        across /= np.linalg.norm(across)
        along = np.array([-across[1], across[0]])
        xy = np.c_[cap[:, :2] @ across, cap[:, :2] @ along * 1e9]
        ax.plot(
            xy[:, 0], xy[:, 1], "o-", label="analytic-fit cap on existing XY domain"
        )
        ax.scatter(
            [0], [0], marker="x", color="crimson", label="original geographic ray"
        )
        span = max(abs(xy[:, 1]).max(), 0.03)
        ax.set_ylim(
            min(0, xy[:, 1].min()) - span / 4, max(0, xy[:, 1].max()) + span / 4
        )
        ax.set_xlabel("across cap (m)")
        ax.set_ylabel("along-source displacement (nm)")
        ax.set_title(
            f"source {row['feature']} / component end {row['component_end']}\nray: {row['geographic_ray']['state']}"
        )
        ax.grid(alpha=0.2)
    axes[0, 0].legend(fontsize=8)
    fig.suptitle(
        "Proposed fit-domain cap. Unequal axis scales. No published mesh/lease or bridge admission."
    )
    fig.savefig(output / "ground-cap-contact-proposal.png", dpi=160)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="explicit system-temp output directory",
    )
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    result = compare(args.output)
    if args.plot:
        plot(result, args.output)
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "old_geographic_ray_still_pending",
                    "cap_work",
                    "DEM_calls",
                    "DEM_samples",
                )
            }
        )
    )
