"""Current Python building procedures on the provider-selected compact flat city windows."""

import argparse
import hashlib
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
import shapely
from city_survey_sources import CITIES, projector, read
from context import attach
from evidence import revision
from outshine_ref.places import decode_vector
from showcase_tiles import frame_proof, view_for
from tile_buildings import construct
from tile_stepped import SteppedDetails

SCENES = ("Paris", "Rom", "Berlin", "London")


def inputs(city, census):
    selected = next(row for row in census["cities"] if row["city"] == city)
    buildings, _, sources = read(city, False)
    expected = {r["url"]: r["sha256"] for r in selected["receipts"]}
    if {r["url"]: r["sha256"] for r in sources["receipts"]} != expected:
        raise ValueError("City source snapshot differs from the inspected census")
    landuse = []
    for receipt in sources["receipts"]:
        tile = receipt["tile"]
        project = projector(city, tile)
        layers = decode_vector(receipt)
        hosts = [r for r in buildings if r["source_tile"] == list(tile)]
        attach(hosts, layers, project, receipt["sha256"])
        for row in hosts:
            row.update(
                layer="building",
                clipped=False,
                placement_base_enu_m=0.0,
                ground_asl_m=None,
                placement_origin="flat shared building plane; no DEM",
            )
        layer = layers.get("landuse", {})
        extent = layer.get("extent", 4096)
        for feature in layer.get("features", ()):
            geometry = shapely.from_geojson(json.dumps(feature["geometry"]))
            for piece in shapely.get_parts(geometry):
                if (
                    piece.geom_type == "Polygon"
                    and piece.is_valid
                    and not piece.is_empty
                ):
                    polygon = shapely.transform(
                        piece,
                        lambda xy, size=extent, p=project: p(xy, size),
                    )
                    landuse.append(
                        {
                            "layer": "landuse",
                            "polygon": polygon,
                            "properties": feature["properties"],
                        }
                    )
    window = selected["windows"][0]
    x, y = window["centre_xy_m"]
    half = window["width_m"] / 2
    chosen = [
        r
        for r in buildings
        if abs(r["xy"][0] - x) <= half and abs(r["xy"][1] - y) <= half
    ]
    if len(chosen) != window["building_polygons"]:
        raise ValueError("Actual city selection differs from its inspected census")
    return chosen + landuse, sources, window


def build(city, census, output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("City scenes require a frozen clean Python implementation")
    rows, sources, window = inputs(city, census)
    view = replace(view_for(rows), width=1920, height=1080)
    procedures = SteppedDetails(rows, view)
    mesh, source = construct(
        city,
        prepared=(rows, {"invalid": [], "span_m": window["width_m"]}),
        ordinary_builder=procedures,
    )
    framing = frame_proof(mesh, view)
    if source["generated"] + len(source["omissions"]) != window["building_polygons"]:
        raise ValueError("Selected city building source is unaccounted")
    output.mkdir(parents=True, exist_ok=True)
    path = output / (city + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    camera = {
        "projection": "perspective",
        "eye": view.eye.tolist(),
        "target": view.target.tolist(),
        "vertical_fov_deg": view.fov_deg,
        "clip_near_m": 0.1,
        "clip_far_m": 10000.0,
        "profile": [view.width, view.height],
    }
    record = proof | {
        "representation": "python-city-building-study",
        "city": city,
        "ground_mode": "flat-no-dem",
        "source_frame": {
            "origin_lat_lon": CITIES[city],
            "axes": "east/north/up metres; latitude-scaled Mercator XY, flat Z",
        },
        "window": window,
        "sources": sources,
        "source": source,
        "camera": camera,
        "camera_framing": framing,
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "triangles": len(mesh["faces"]),
        "facade_materials": procedures.facades.bindings,
        "procedures": {
            "stepped": procedures.report(),
            "facades": procedures.facades.report(),
            "roofs": procedures.facades.base.report(),
            "rooms": procedures.facades.base.rooms.report(),
        },
        "scope": "Current Python research geometry on actual selected source polygons; simple generic room controls, no completed architecture-family/native/LOD/device admission",
    }
    (output / (city + "-geometry.json")).write_text(json.dumps(record, indent=2) + "\n")
    print(
        city,
        source["generated"],
        "building polygons",
        len(mesh["faces"]),
        "triangles",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cities", nargs="+", choices=SCENES, default=list(SCENES))
    args = parser.parse_args()
    census = json.loads(args.census.read_text())
    for city in args.cities:
        build(city, census, args.output)
