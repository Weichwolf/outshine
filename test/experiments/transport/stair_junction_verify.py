"""Replay complete saved stair bodies, source fields, landings and proposed ports."""

import itertools
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from shared_bridge_geometry import corridor
from stair_junction_audit import audit
from stair_junction_experiment import arguments, outer_ports
from stair_junction_layout import inputs, landings


def source_fields(arrays, data, rows, domains):
    fields = [
        Mesh(np.c_[arrays["xy"], arrays["heights"]], arrays["faces" + str(i)])
        for i in range(4)
    ]
    for owner, field in enumerate(fields):
        polygons = shapely.polygons(field.vertices[field.triangles, :2])
        if (
            not shapely.coverage_is_valid(polygons)
            or shapely.union_all(polygons).symmetric_difference(domains[owner]).area
            > 1e-7
        ):
            raise ValueError("saved solver field changed a whole source footprint")
        selected = (data["face_sources"] & (1 << owner)) != 0
        actual = Mesh(data["reference_vertices"], data["reference_faces"][selected])
        for a, b in ((field, actual), (actual, field)):
            note = vertical_clearance(a, b, seconds=5, max_faces=4096)
            if not note["valid"] or abs(note["minimum_clearance_m"]) > 1e-7:
                raise ValueError(
                    "actual saved whole envelope differs from measured solver field"
                )
    node = rows[0]["xy"][0]
    sample = SurfaceSampler(fields[0]).sample([node])
    if (
        any(sample.reasons)
        or abs(sample.points_enu_m[0, 2] - rows[0]["prior"][0]) > 1e-7
    ):
        raise ValueError("saved source frame datum changed")
    root = rows[0]["prior"][0]
    if (
        arrays["heights"].min() < root - 2 - 1e-7
        or arrays["heights"].max() > root + 10 + 1e-7
    ):
        raise ValueError("saved finite source height budget exceeded")
    return fields


def landing_proof(fields, rows, domains):
    nodes, incident, masks = landings(rows, domains, "width", 0.75)
    error = 0.0
    for node, sources, mask in zip(nodes, incident, masks, strict=True):
        sample = SurfaceSampler(fields[0]).sample([node])
        if any(sample.reasons):
            raise ValueError("saved stair landing has no endpoint surface")
        height = sample.points_enu_m[0, 2]
        for owner in sources:
            field = fields[owner]
            points = field.vertices[field.triangles]
            pieces = shapely.intersection(shapely.polygons(points[:, :, :2]), mask)
            for i in np.flatnonzero(shapely.area(pieces) > 1e-12):
                tri = points[i]
                normal = np.cross(tri[1] - tri[0], tri[2] - tri[0])
                gradient = -normal[:2] / normal[2]
                xy = shapely.get_coordinates(pieces[i])
                error = max(
                    error,
                    float(abs(tri[0, 2] + (xy - tri[0, :2]) @ gradient - height).max()),
                )
    if error > 1e-7:
        raise ValueError("whole saved common landing is not flat and shared")
    field = fields[0]
    points = field.vertices[field.triangles]
    shapes = shapely.polygons(points[:, :, :2])
    n = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    gradients = -n[:, :2] / n[:, 2, None]
    backward = 0.0
    for a, b in itertools.pairwise(rows[0]["xy"]):
        t = (b - a) / np.linalg.norm(b - a)
        hit = (
            shapely.area(
                shapely.intersection(
                    shapes, shapely.LineString([a, b]).buffer(rows[0]["width_m"])
                )
            )
            > 1e-10
        )
        backward = max(backward, float((-gradients[hit] @ t).max()))
    if backward > 1e-7:
        raise ValueError("saved stair envelope turns backwards along source frames")
    return {"whole_flat_contact_error_m": error, "backward_grade_violation": backward}


def display_proof(args, data, report):
    if digest(args.palette_scene) != report["presentation"]["palette_scene_sha256"]:
        raise ValueError("saved presentation palette binding changed")
    path = args.output / "display-body.npz"
    if digest(path) != report["presentation"]["mesh_sha256"]:
        raise ValueError("saved indexed display body binding changed")
    with np.load(path) as saved:
        d = dict(saved)
    v, f = data["body_vertices"], data["body_faces"]
    if not np.array_equal(d["vertices"], v) or not np.array_equal(d["triangles"], f):
        raise ValueError("presentation changes actual verified stair geometry")
    slots = np.full(len(f), 3)
    stairs = (data["face_sources"] & 1) != 0
    slots[np.r_[stairs, stairs, data["wall_riser"] == 1]] = 10
    palette = np.array(json.loads(args.palette_scene.read_text())["colours"])
    xyz = v[f]
    n = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    n /= np.linalg.norm(n, axis=1)[:, None]
    if (
        not np.array_equal(d["materials"], slots)
        or not np.array_equal(d["face_base_colour_linear_rgba"], palette[slots])
        or not np.array_equal(d["normals"], np.repeat(n, 3, axis=0))
    ):
        raise ValueError("saved stair presentation normal/colour ownership differs")


def verify(args):
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            str(Path(__file__).resolve().parent),
        ],
        text=True,
    ).strip():
        raise ValueError("saved four-way replay requires frozen clean research")
    report = json.loads((args.output / "comparison.json").read_text())
    if report["dirty"] or digest(args.input) != report["input_sha256"]:
        raise ValueError("dirty or unbound actual four-way input")
    for file, key in (
        ("body.npz", "body_sha256"),
        ("height-input.npz", "height_input_sha256"),
    ):
        if digest(args.output / file) != report[key]:
            raise ValueError("saved actual stair/envelope export binding changed")
    with np.load(args.output / "body.npz") as saved:
        data = dict(saved)
    with np.load(args.output / "height-input.npz") as saved:
        arrays = dict(saved)
    _case, rows = inputs(args.input)
    domains = [corridor(r) for r in rows]
    proof = audit(data, domains)
    fields = source_fields(arrays, data, rows, domains)
    proof["landings"] = landing_proof(fields, rows, domains)
    proposed = outer_ports(
        {
            "rows": rows,
            "xy": arrays["xy"],
            "parts": [arrays["faces" + str(i)] for i in range(4)],
        },
        arrays["heights"],
    )
    if proposed != report["outer_ports"] or report["source_ids"] != [
        r["source_id"] for r in rows
    ]:
        raise ValueError("saved source/outer-port ownership changed")
    display_proof(args, data, report)
    proof.update(
        valid=True,
        generation_commit=report["commit"],
        reader_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        outer_ports=proposed,
        status="valid_local_recipe_pending_actual_outer_contacts",
    )
    (args.output / "verification.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    verify(arguments())
