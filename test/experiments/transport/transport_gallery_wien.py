"""Fresh 3D gallery of the verified bridge/approach/tunnel research, with two details."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from capture_tunnel_portals import FIXTURE, PIN, base_surfaces
from ground_surface_showcase import combined
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from surface_solid_checks import audit_closed
from surface_solid_showcase import inspection_view
from transport_place_inputs import GIRTH_M, mercator
from tunnel_portal_experiment import complete, source_usage
from tunnel_portal_showcase import assembled
from whole_tile_showcase import COLOURS, tile_frame

HERE = Path(__file__).resolve().parent


def replay(model, previous, note):
    sampled = SurfaceSampler(model["top"]).sample(previous[0].vertices[:, :2])
    if any(sampled.reasons) or not np.allclose(
        sampled.points_enu_m, previous[0].vertices, rtol=0, atol=1e-7
    ):
        raise ValueError("fresh research lost a previous chosen contact")
    gap = vertical_clearance(
        model["ground"],
        Mesh(model["top"].vertices - [0, 0, 0.35], model["top"].triangles),
    )
    if not gap["valid"] or gap["minimum_clearance_m"] < 4.5 - 1e-7:
        raise ValueError("whole bridge/ground separation lost")
    if (
        note["cover"]["minimum_clearance_m"] < 2
        or note["headroom"]["minimum_clearance_m"] < 4.5 - 1e-7
    ):
        raise ValueError("roof cover or lining air section lost")
    return {
        "old_contacts_preserved": True,
        "whole_bridge_clearance": gap,
        "minimum_roof_cover_m": note["cover"]["minimum_clearance_m"],
        "minimum_tunnel_air_m": note["headroom"]["minimum_clearance_m"],
    }


def regions(raw):
    result = []
    origin = raw["origin_lat_lon"]
    for receipt in raw["receipts"]:
        if digest(Path(receipt["path"])) != receipt["sha256"]:
            raise ValueError("research source bytes changed")
        z, x, y = receipt["tile"]
        span = GIRTH_M / (1 << z)
        xy = np.array(
            [
                [tx * span - GIRTH_M / 2, GIRTH_M / 2 - ty * span]
                for tx, ty in ((x, y + 1), (x + 1, y + 1), (x + 1, y), (x, y))
            ]
        )
        xy = (xy - mercator(*origin)) * np.cos(np.radians(origin[0]))
        result.append(shapely.Polygon(xy))
    return result


def render(name, mesh, slots, output, profile, direction):
    path = output / (name + ".npz")
    np.savez_compressed(
        path, vertices=mesh.vertices, triangles=mesh.triangles, materials=slots
    )
    view = inspection_view(mesh.vertices, eye_direction=direction)
    view["profile"] = profile
    row = {
        "id": name,
        "mesh": str(path),
        "mesh_sha256": digest(path),
        "output": str(output / (name + ".png")),
        "colours": COLOURS,
        **view,
    }
    scene = output / (name + "-scene.json")
    scene.write_text(json.dumps([row], indent=2) + "\n")
    subprocess.run(
        [
            "nice",
            "-n",
            "10",
            "blender",
            "--background",
            "--factory-startup",
            "--threads",
            "2",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scene),
        ],
        check=True,
        timeout=90,
    )
    return {
        "scene": row,
        "closed_volume_m3": audit_closed(mesh),
        "image_sha256": digest(output / (name + ".png")),
    }


def build(reference, capture, output):
    began = time.perf_counter()
    if digest(FIXTURE) != PIN:
        raise ValueError("pinned portal fixture changed")
    fixture = json.loads(FIXTURE.read_text())
    base = (
        reference / "bridge-continuations/Vienna-continuation-whole-rebuild-solid.npz"
    )
    previous, origin = base_surfaces(base)
    if origin != fixture["base"]:
        raise ValueError(
            "previous bridge/approach plan differs from chosen tunnel contact"
        )
    model, note, _plan, binding, solid, slots = complete(fixture, previous, "cubic")
    proof = replay(model, previous, note)
    raw = json.loads((capture / "inputs/ViennaResearch-input.json").read_text())
    usage = source_usage(fixture, note)
    actual_ids = {r["source_id"] for r in raw["roads"] + raw["deferred_sources"]}
    if {r["source_id"] for r in usage} != actual_ids:
        raise ValueError(
            "research usage does not account for the full three source tiles"
        )
    if raw["origin_lat_lon"] != fixture["origin_lat_lon"]:
        raise ValueError("research and captured source frame differ")
    frames = tile_frame(regions(raw), float(solid.vertices[:, 2].min() - 2))
    whole, colours = combined([(solid, slots), frames])
    views = {
        "whole": render(
            "Wien-whole", whole, colours, output, [1280, 720], (-0.3, -1, 0.85)
        )
    }
    bridge, bridge_slots = assembled(
        model, previous, region=shapely.box(3720, -95, 3900, 85)
    )
    views["bridge"] = render(
        "Wien-bridge", bridge, bridge_slots, output, [384, 216], (-0.3, -1, 0.45)
    )
    field_origin, axis, _table = model["field"]
    portal = field_origin + axis * note["field"]["roof_start_m"]
    tunnel, tunnel_slots = assembled(
        model,
        previous,
        region=shapely.box(*(portal - [65, 35]), *(portal + [65, 35])),
        cutaway=True,
    )
    views["portal"] = render(
        "Wien-portal", tunnel, tunnel_slots, output, [384, 216], (0.8, -0.8, 0.6)
    )
    from PIL import Image, ImageDraw, ImageFont

    with Image.open(output / "Wien-whole.png") as source:
        image = source.convert("RGB")
    for name, position in (("bridge", (888, 8)), ("portal", (888, 496))):
        with Image.open(output / ("Wien-" + name + ".png")) as detail:
            image.paste(detail.convert("RGB"), position)
        x, y = position
        ImageDraw.Draw(image).rectangle(
            (x - 1, y - 1, x + 384, y + 216), outline=(100, 110, 110)
        )
        draw = ImageDraw.Draw(image)
        draw.rectangle((x, y, x + 383, y + 22), fill=(244, 245, 242))
        draw.text(
            (x + 6, y + 3),
            "Brücke" if name == "bridge" else "Tunnel · Schnitt",
            font=ImageFont.load_default(size=14),
            fill=(45, 55, 55),
        )
    image.save(output / "Wien.png")
    receipt = {
        "fixture_sha256": PIN,
        "previous": origin,
        "verification": proof,
        "recipe": note,
        "source_usage": usage,
        "spatial_binding": binding,
        "views": views,
        "image_sha256": digest(output / "Wien.png"),
        "seconds": time.perf_counter() - began,
        "manual_review_required": True,
        "scope": "Fresh unchanged Python bridge/approach/tunnel research in its real three source tiles. Insets are actual 3D closeups; portal uses the existing explicit lining/terrain cutaway. No height exaggeration or native screenshot. Complete common terrain/other tile traffic, piers, train routing and native integration remain open.",
    }
    (output / "Wien-research.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(
        json.dumps(
            {"place": "Wien", "seconds": receipt["seconds"], "verification": proof}
        ),
        flush=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.reference_root, args.capture, args.output)


if __name__ == "__main__":
    main()
