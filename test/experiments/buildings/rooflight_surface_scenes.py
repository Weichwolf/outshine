"""Far and complete-source scene controls for checked rooflight contact skins."""

import argparse
import hashlib
import json
from contextlib import contextmanager
from pathlib import Path
from unittest.mock import patch

import numpy as np
from demand import View
from evidence import revision
from industrial_hall_details import products
from industrial_hall_shell import structure
from industrial_rooflight_probe import prototype
from rooflight_surface_probe import emit, equal, mapped, prepared
from rooflight_surfaces import closed, contact, prepare_host
from tile_buildings import construct, read_tile
from tile_facades import contexts
from tile_stepped import SteppedDetails
from verify_industrial_halls import sources


def write(record, mesh, source, output, proof, method, procedures):
    output.mkdir(parents=True, exist_ok=True)
    path = output / (record["city"] + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    value = (
        record
        | proof
        | {
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "source": source,
            "triangles": len(mesh["faces"]),
            "surface_method": method,
            "procedures": procedures,
            "scope": "Actual prepared source body and unchanged palette/camera; direct rooflight product. Contact surfaces require the opaque mating body; this open render skin is not an independent collider or a whole-building LOD admission.",
        }
    )
    (output / (record["city"] + "-geometry.json")).write_text(
        json.dumps(value, indent=2) + "\n"
    )


def far(reference, output, proof):
    record = json.loads(reference.read_text())
    camera = dict(record["camera"])
    eye, target = np.asarray(camera["eye"]), np.asarray(camera["target"])
    camera["eye"] = (target + (eye - target) * 4).tolist()
    record = record | {
        "city": "Rooflight2FarGlazed",
        "camera": camera,
        "prior_reference": str(reference),
    }
    view = View(
        np.asarray(camera["eye"]),
        target,
        width=1920,
        height=1080,
        fov_deg=camera["vertical_fov_deg"],
    )
    row, neighbors = sources()[2]
    value = prepared(row, neighbors)
    for method in ("cga", "closed", "contact"):
        mesh, source = mapped(row, emit(value, view, method), record["city"])
        write(
            record,
            mesh,
            source,
            output / method,
            proof,
            method,
            {
                "source_id": row["id"],
                "fields": len(value["fields"]),
                "prepared_body_triangles": len(value["body"]["faces"]),
                "original_camera_reference": str(reference),
            },
        )


@contextmanager
def variant(method):
    holder = {}

    def build(value, envelope, material):
        from industrial_rooflights import cuts

        body, shell = structure(
            value, envelope, material, extra_cuts=cuts(value["rooflights"], material)
        )
        if method == "contact":
            holder[value["id"]] = prepare_host(
                shell, value["rooflights"], material=material
            )
        return body, shell

    def parts(value, material, opened, view):
        previous, detailed = products(value, material, opened, view)
        extra = (
            contact(holder.pop(value["id"]))
            if method == "contact"
            else closed(value["rooflights"])
        )
        return [*previous, *extra], detailed

    with prototype():
        if method == "cga":
            yield
        else:
            with (
                patch("tile_industrial_halls.structure", side_effect=build),
                patch("tile_industrial_halls.products", side_effect=parts),
            ):
                yield
    if holder:
        raise ValueError("Unconsumed prepared rooflight host")


def tile(reference, output, proof):
    record = json.loads(reference.read_text())
    camera = record["camera"]
    view = View(
        np.asarray(camera["eye"]),
        np.asarray(camera["target"]),
        width=1920,
        height=1080,
        fov_deg=camera["vertical_fov_deg"],
    )
    rows, receipt = read_tile("Grohnde")
    contexts(rows, receipt)
    old = None
    for method in ("cga", "contact"):
        owner = SteppedDetails(rows, view)
        with variant(method):
            mesh, source = construct(
                "Grohnde", prepared=(rows, receipt), ordinary_builder=owner
            )
        if method == "cga":
            equal(mesh, record, source)
            old = source
            continue
        for key in ("generated", "omissions", "counts", "material_definitions"):
            if old[key] != source[key]:
                raise ValueError("Contact skin changed source coverage/palette")
        first = [
            {k: v for k, v in r.items() if k != "prototype"} for r in old["members"]
        ]
        last = [
            {k: v for k, v in r.items() if k != "prototype"} for r in source["members"]
        ]
        if first != last:
            raise ValueError(
                "Contact skin changed source identities/attributes/footprints/heights"
            )
        changed = record | {
            "prior_reference": str(reference),
            "facade_materials": owner.facades.bindings,
        }
        write(
            changed,
            mesh,
            source,
            output / method,
            proof,
            method,
            {
                "facades": owner.facades.report(),
                "roofs": owner.facades.base.report(),
                "cga_reference_arrays_exact": True,
            },
        )
        print(
            source["generated"],
            len(mesh["faces"]),
            len(owner.facades.halls.completed),
            owner.facades.halls.rejected,
            flush=True,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--far-reference", type=Path)
    parser.add_argument("--tile-reference", type=Path)
    args = parser.parse_args()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze source before scene controls")
    if args.far_reference:
        far(args.far_reference, args.output, proof)
    if args.tile_reference:
        tile(args.tile_reference, args.output, proof)
