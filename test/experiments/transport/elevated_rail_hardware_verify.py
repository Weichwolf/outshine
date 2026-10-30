"""Independent whole-source, metric profile, finite bed and instance verification."""

import argparse
import json
import zipfile
from dataclasses import asdict
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from elevated_rail_hardware import MATERIALS, bound_input, selected_bed
from elevated_rail_joints import joints
from experiment_output import create_output
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import RailBudget, RailRecipe
from outshine_ref.transport.rail_hardware._geometry import SECTION, SLEEPER
from outshine_ref.transport.rail_hardware._planar import projected_rail_footprints
from rail_hardware_checks import closed_volume
from rail_place_verify import independent_sleepers, verify_piece
from stair_module_inputs import reference_profiles
from stair_terrain_experiment import frozen


def arrays(path, names):
    with zipfile.ZipFile(path) as archive:
        if sum(item.file_size for item in archive.infolist()) > 32 * 1024**2:
            raise ValueError("Bounded whole hardware payload required before decode")
    with np.load(path, allow_pickle=False) as saved:
        if set(saved.files) != set(names):
            raise ValueError("Exact complete hardware array inventory required")
        result = {key: saved[key] for key in names}
    if any(not np.isfinite(value).all() for value in result.values()):
        raise ValueError("Finite complete hardware arrays required")
    for value in result.values():
        value.setflags(write=False)
    return result


def source_axis(record, originals, allowed):
    points, edges, keys, previous = [], [], [], None
    for member in record["members"]:
        key, reverse = member["source_id"], member["reversed"]
        if key not in originals or key in keys or type(reverse) is not bool:
            raise ValueError("Distinct original whole member and direction required")
        row = originals[key]
        if row["original_properties"].get("gauge") not in (None, ""):
            raise ValueError(
                "Supplied gauge cannot be silently replaced by the default recipe"
            )
        axis = row["xy"][::-1] if reverse else row["xy"]
        nodes = row["original_mvt_xy"][::-1] if reverse else row["original_mvt_xy"]
        if previous is not None and (
            not np.array_equal(points[-1], axis[0])
            or previous[1] != nodes[0]
            or frozenset((previous[0], key)) not in allowed
        ):
            raise ValueError("Existing whole physical joint identity required")
        points.extend(axis if not points else axis[1:])
        edges.extend([key] * (len(axis) - 1))
        keys.append(key)
        previous = key, nodes[-1]
    axis = np.asarray(points)
    if (
        not keys
        or not np.array_equal(axis, record["axis_xy_m"])
        or edges != record["segment_sources"]
    ):
        raise ValueError("Every complete original axis segment must be retained")
    return axis, keys


def profile(rail, axis, datum, recipe):
    rings = rail.vertices.reshape(2, len(axis), 12, 3)
    tangent = np.diff(axis, axis=0)
    tangent /= np.linalg.norm(tangent, axis=1)[:, None]
    normals = np.c_[-tangent[:, 1], tangent[:, 0]]
    along = np.vstack((tangent[0], tangent[:-1] + tangent[1:], tangent[-1]))
    along /= np.linalg.norm(along, axis=1)[:, None]
    maximum = 0.0
    for side in range(2):
        displacement = rings[side, :, :, :2] - axis[:, None]
        maximum = max(
            maximum, float(abs(np.einsum("nvi,ni->nv", displacement, along)).max())
        )
        wanted = (side * 2 - 1) * (recipe.gauge_m + 0.072) / 2 + SECTION[:, 0]
        for values in (displacement[:-1], displacement[1:]):
            error = np.einsum("nvi,ni->nv", values, normals) - wanted
            maximum = max(maximum, float(abs(error).max()))
    z_error = abs(rings[..., 2] - (datum + recipe.sleeper_height_m + SECTION[:, 1]))
    maximum = max(maximum, float(z_error.max()))
    if maximum > 1e-9:
        raise ValueError(
            "Saved whole section differs from original metric axis/profile"
        )
    return maximum


def instances(transforms, axis, recipe):
    lengths = np.linalg.norm(np.diff(axis, axis=0), axis=1)
    total = float(lengths.sum())
    count = (
        int(np.floor((total - recipe.sleeper_width_m) / recipe.sleeper_spacing_m)) + 1
    )
    if (
        not 1 <= count <= RailBudget().maximum_sleepers
        or not 1 <= len(transforms) <= count
    ):
        raise ValueError("Bounded complete intended sleeper positions required")
    first = (total - (count - 1) * recipe.sleeper_spacing_m) / 2
    line = shapely.LineString(axis)
    centres = transforms[:, :2, 3]
    distance = shapely.line_locate_point(line, shapely.points(centres))
    slots = np.rint((distance - first) / recipe.sleeper_spacing_m).astype(int)
    if np.any(slots < 0) or np.any(slots >= count) or np.any(np.diff(slots) <= 0):
        raise ValueError("Distinct ordered original sleeper proposals required")
    wanted = first + slots * recipe.sleeper_spacing_m
    expected = shapely.get_coordinates(shapely.line_interpolate_point(line, wanted))
    segments = np.minimum(
        np.searchsorted(np.cumsum(lengths), wanted, side="right"), len(lengths) - 1
    )
    tangent = np.diff(axis, axis=0)[segments] / lengths[segments, None]
    error = max(
        float(abs(centres - expected).max()),
        float(
            abs(
                transforms[:, :2, 0]
                - np.c_[-tangent[:, 1], tangent[:, 0]] * recipe.sleeper_length_m
            ).max()
        ),
        float(abs(transforms[:, :2, 1] + tangent * recipe.sleeper_width_m).max()),
        float(abs(transforms[:, 2, :2]).max()),
        float(abs(transforms[:, :2, 2]).max()),
        float(abs(transforms[:, 2, 2] - recipe.sleeper_height_m).max()),
    )
    if error > 1e-9:
        raise ValueError(
            "Actual saved sleeper dimensions, orientation or source phase differ"
        )
    vertices = independent_sleepers(transforms).reshape(-1, 8, 3)
    footprints = shapely.polygons(vertices[:, :4, :2])
    missing = np.setdiff1d(np.arange(count), slots)
    tree = shapely.STRtree(footprints)
    for slot in missing:
        point = shapely.get_coordinates(
            shapely.line_interpolate_point(
                line, first + slot * recipe.sleeper_spacing_m
            )
        )[0]
        segment = min(
            int(
                np.searchsorted(
                    np.cumsum(lengths),
                    first + slot * recipe.sleeper_spacing_m,
                    side="right",
                )
            ),
            len(lengths) - 1,
        )
        direction = np.diff(axis, axis=0)[segment] / lengths[segment]
        lateral = np.array([-direction[1], direction[0]]) * recipe.sleeper_length_m / 2
        forward = direction * recipe.sleeper_width_m / 2
        shape = shapely.Polygon(
            [
                point - lateral - forward,
                point + lateral - forward,
                point + lateral + forward,
                point - lateral + forward,
            ]
        )
        candidates = tree.query(shape, predicate="intersects")
        if not np.any(
            shapely.area(shapely.intersection(shape, footprints[candidates])) > 0
        ):
            raise ValueError("Missing corner sleeper has no actual retained conflict")
    return vertices, error, len(missing)


def float32_instances(transforms):
    matrices, unit = transforms.astype("f4"), SLEEPER.vertices.astype("f4")
    result = np.zeros((len(matrices), 8, 3), "f4")
    for column in range(3):
        product = np.multiply(
            matrices[:, None, :, column], unit[None, :, column, None], dtype="f4"
        )
        result = np.add(result, product, dtype="f4")
    return np.add(result, matrices[:, None, :, 3], dtype="f4")


def component(data, record, axis, bed, origin, clearance, recipe):
    local, faces, local_transforms = (
        data[key] for key in ("rail_vertices", "rail_triangles", "sleeper_transforms")
    )
    datum = float(origin[2])
    if (
        local.dtype != np.dtype("<f8")
        or faces.dtype != np.dtype("<u4")
        or local_transforms.dtype != np.dtype("<f8")
    ):
        raise ValueError("Original declared reference scalar/index types required")
    if (
        local.shape != (24 * len(axis), 3)
        or local_transforms.ndim != 3
        or local_transforms.shape[1:] != (3, 4)
    ):
        raise ValueError("Complete whole rail sections/instances required")
    v = local + origin
    transforms = local_transforms.copy()
    transforms[:, :, 3] += origin
    if (
        len(v) > RailBudget().maximum_rail_vertices
        or len(transforms) > RailBudget().maximum_sleepers
    ):
        raise ValueError("Bounded actual hardware counts required")
    rail = Mesh(v, faces)
    section_error = profile(rail, axis, datum, recipe)
    sleepers, instance_error, omitted = instances(transforms, axis, recipe)
    box_faces = (
        SLEEPER.triangles[None]
        + len(v)
        + 8 * np.arange(len(transforms), dtype=np.uint32)[:, None, None]
    )
    all_vertices = np.vstack((v, sleepers.reshape(-1, 3)))
    all_faces = np.vstack((faces, box_faces.reshape(-1, 3)))
    note = {
        "rail_vertices": len(v),
        "rail_triangles": len(faces),
        "sleepers": len(transforms),
        "gauge_m": recipe.gauge_m,
    }
    shapes, _line, gauge, _box_error = verify_piece(
        all_vertices, all_faces, transforms, note, datum
    )
    if (
        not shapely.covers(bed, shapes).all()
        or all_vertices[:, 2].min() < datum - 1e-9
        or all_vertices[:, 2].max() > datum + clearance
    ):
        raise ValueError(
            "Whole hardware must stay supported and inside old reserved volume"
        )
    rounded = sleepers.astype("f4").astype(float)
    rounded_rail = Mesh(v.astype("f4").astype(float), faces)
    rounded_shapes = np.array(
        [
            *projected_rail_footprints(rounded_rail),
            *shapely.polygons(rounded[:, :4, :2]),
        ],
        object,
    )
    if (
        not shapely.covers(bed, rounded_shapes).all()
        or closed_volume(rounded_rail) <= 0
    ):
        raise ValueError("Actual rounded hardware escaped the finite bed or collapsed")
    local32 = float32_instances(local_transforms)
    local_foot32 = local.astype("f4")[:, 2].min()
    contact32 = abs(local32[:, 4:, 2] - local_foot32).max()
    world32 = np.add(local32, origin.astype("f4"), dtype="f4").astype(float)
    rail32 = np.add(local.astype("f4"), origin.astype("f4"), dtype="f4").astype(float)
    instance_shapes = np.array(
        [
            *projected_rail_footprints(Mesh(rail32, faces)),
            *shapely.polygons(world32[:, :4, :2]),
        ],
        object,
    )
    if contact32 != 0 or np.any(world32[:, 4:, 2] != rail32[:, 2].min()):
        raise ValueError(
            "Shared height datum must preserve actual Float32 running contact"
        )
    if not shapely.covers(bed, instance_shapes).all() or world32[
        :, :, 2
    ].min() != float(np.float32(datum)):
        raise ValueError("Declared Float32 instances must keep finite bed support")
    stats = {
        "rail_vertices": len(v),
        "rail_faces": len(faces),
        "sleepers": len(transforms),
        "metric_profile_error_m": section_error,
        "metric_gauge_error_m": gauge,
        "independent_instance_error_m": instance_error,
        "justified_corner_omissions": omitted,
        "whole_double_and_rounded_footprints_supported": True,
        "contained_in_existing_flat_reserved_volume": True,
        "literal_float32_instance_contact_error_m": float(contact32),
        "native_gpu_instance_arithmetic_qualified": False,
    }
    return shapes, (rounded_shapes, instance_shapes), stats


def verify(root):
    report = json.loads((root / "report.json").read_text())
    if (
        report.get("version") != 2
        or report["recipe"] != asdict(RailRecipe())
        or report["budget"] != asdict(RailBudget())
        or report["material_recipe"] != list(MATERIALS)
    ):
        raise ValueError(
            "Current explicit complete hardware schema/recipe/budget required"
        )
    building, receipt = Path(report["buildings"]), Path(report["building_receipt"])
    if (
        digest(building / "comparison.json") != report["building_report_sha256"]
        or digest(receipt) != report["building_receipt_sha256"]
    ):
        raise ValueError("Exact saved whole consumer input identity required")
    products = bound_input(building, receipt)
    source_rows = [{k: v for k, v in row.items() if k != "xy"} for row in products.rows]
    if (
        report["frame"] != products.state[4]["frame"]
        or report["ground_source_ids"] != products.state[4]["source_ids"]
        or report["raised_source_rows"] != source_rows
    ):
        raise ValueError("Original whole source/frame/role binding changed")
    records = report["components"]
    if not isinstance(records, list) or len(records) > 8:
        raise ValueError("Bounded whole hardware components required")
    expected = {f"component-{i}.npz" for i in range(len(records))}
    if records:
        expected.add("sleeper-template.npz")
    if set(report["artifacts"]) != expected:
        raise ValueError("Complete exact hardware artifacts required")
    for name, pin in report["artifacts"].items():
        if digest(root / name) != pin:
            raise ValueError("Saved hardware artifact changed")
    if records:
        template = arrays(root / "sleeper-template.npz", ("vertices", "triangles"))
        if not np.array_equal(
            template["vertices"], SLEEPER.vertices
        ) or not np.array_equal(template["triangles"], SLEEPER.triangles):
            raise ValueError("One unchanged common unit sleeper required")
    bed, triangle = selected_bed(products)
    datum = float(triangle[0, 2])
    if datum != report["datum_m"]:
        raise ValueError("Actual selected bed datum changed")
    origin = np.asarray(report["object_origin_m"], float)
    if not np.array_equal(origin, [0.0, 0.0, datum]) or not np.array_equal(
        report["object_axes"], np.eye(3)
    ):
        raise ValueError("The actual common local bed object frame is required")
    if report["running_surface_offset_m"] != RailRecipe().sleeper_height_m + 0.172:
        raise ValueError(
            "Actual running surface height is not a new vehicle-clearance grant"
        )
    originals = {r["source_id"]: r for r in products.rows}
    allowed = {
        frozenset(products.rows[i]["source_id"] for i in j["indices"])
        for j in joints(products.rows)
    }
    clearance = min(
        reference_profiles()[r["original_properties"]["subclass"]]["clearanceM"]
        for r in products.rows
    )
    accepted, all_shapes, rounded_shapes, stats = {}, [], [], []
    for index, record in enumerate(records):
        name = f"component-{index}.npz"
        if record["file"] != name or record["sha256"] != report["artifacts"][name]:
            raise ValueError("Exact ordered whole component payload required")
        axis, keys = source_axis(record, originals, allowed)
        if set(keys) & accepted.keys():
            raise ValueError("A whole source cannot be duplicated across components")
        accepted.update((key, index) for key in keys)
        data = arrays(
            root / name, ("rail_vertices", "rail_triangles", "sleeper_transforms")
        )
        shape, rounded, note = component(
            data, record, axis, bed, origin, clearance, RailRecipe()
        )
        all_shapes.append(shape)
        rounded_shapes.append(rounded)
        stats.append(note)
    if (
        sum(r["sleepers"] for r in stats) > RailBudget().maximum_sleepers
        or sum(r["rail_vertices"] for r in stats) > RailBudget().maximum_rail_vertices
    ):
        raise ValueError("Whole actual field payload exceeds its recorded budget")
    if len(report["omitted"]) > 8 or any(
        r.get("source_id") not in originals
        or not isinstance(r.get("reason"), str)
        or not r["reason"]
        for r in report["omitted"]
    ):
        raise ValueError("Every omitted source needs an actual explicit reason")
    omitted = [
        r["source_id"]
        for r in report["omitted"]
        if isinstance(r.get("reason"), str) and r["reason"]
    ]
    if (
        len(set(omitted)) != len(omitted)
        or set(omitted) & accepted.keys()
        or accepted.keys() | set(omitted) != originals.keys()
    ):
        raise ValueError(
            "Every whole source needs one product or explicit local absence"
        )
    for pair in allowed:
        a, b = pair
        if accepted.get(a) != accepted.get(b):
            raise ValueError(
                "Constructed joined sources must remain one complete hardware chain"
            )
    pair_counts = []
    for shapes in (
        all_shapes,
        [r[0] for r in rounded_shapes],
        [r[1] for r in rounded_shapes],
    ):
        flat = np.concatenate(shapes) if shapes else np.empty(0, object)
        kinds = (
            np.concatenate([np.arange(len(s)) >= 2 for s in shapes])
            if shapes
            else np.empty(0, bool)
        )
        tree = shapely.STRtree(flat)
        pairs_seen = 0
        for begin in range(0, len(flat), 64):
            pairs = tree.query(flat[begin : begin + 64], predicate="intersects")
            a, b = pairs[0] + begin, pairs[1]
            pairs_seen += len(a)
            if pairs_seen > 1000000:
                raise ValueError("Whole hardware pair budget exhausted")
            keep = (a < b) & (kinds[a] == kinds[b])
            if np.any(
                shapely.area(shapely.intersection(flat[a[keep]], flat[b[keep]])) > 0
            ):
                raise ValueError("Actual hardware has a same-kind volume intersection")
        pair_counts.append(pairs_seen)
    return {
        "reader_commit": frozen(),
        "producer_commit": report["commit"],
        "report_sha256": digest(root / "report.json"),
        "whole_hardware_verified": True,
        "source_count": len(accepted),
        "components": stats,
        "sleepers": sum(r["sleepers"] for r in stats),
        "omitted": len(omitted),
        "actual_footprint_pair_candidates_double_baked32_instanced32": pair_counts,
        "whole_old_consumers_preserved_by_reserved_volume_containment": True,
        "scope": "Fresh whole saved-source/profile/instance/footprint/contact/reference Float32 replay. No runtime asset-cache hit, native GPU arithmetic, vehicle, Source/World or device admission.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Fresh directory for verification.json",
    )
    args = parser.parse_args()
    create_output(args.output)
    result = verify(args.products)
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
