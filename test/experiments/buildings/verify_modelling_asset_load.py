"""Owned OBJ import, source resources and direct modelling channel controls."""

import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path

import numpy as np

from evidence import revision
from modelling_asset_cases import COLOUR, UV, VERTICES, fixtures, textured_fixture
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from outshine_ref.buildings.modelling.shape import Shape
from modelling_checks import near
from verify_modelling_asset_channels import controls


def emitted(asset):
    return Shape(
        asset.geometry,
        Scope(position=asset.minimum, size=asset.geometry.reference_size),
        asset=asset,
    ).emit()


def obj_control(path):
    asset = Asset.from_file(path)
    mesh = emitted(asset)
    near(mesh["vertices"], VERTICES)
    near(mesh["uv"], UV)
    near(mesh["normals"], [[0, 0, 1]] * 4)
    assert len(mesh["faces"]) == 2 and asset.geometry.group_names == (
        (0, "pane_facade"),
    )
    values = dict(asset.geometry.material_defs[1][1].attributes)
    near(values["kd"], COLOUR[:3])
    assert "owned-pane.mtl" in dict(asset.import_info.dependencies)
    assert asset.import_info.uv_convention == "OBJ lower-left"
    again = Asset.from_file(path)
    assert asset.geometry.polygons == again.geometry.polygons
    assert asset.import_info == again.import_info and asset.resources == again.resources
    assert asset.geometry.material_defs == again.geometry.material_defs
    for key, value in mesh.items():
        np.testing.assert_array_equal(value, emitted(again)[key])
    images = []
    for image in asset.resources.images:
        raw = path.with_name(image.name).read_bytes()
        assert image.encoded == raw
        images.append(
            dict(
                name=image.name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()
            )
        )
    return dict(
        name=path.name,
        groups=asset.geometry.group_names,
        source=asdict(asset.import_info),
        images=images,
    )


def invalid_controls(output, obj_path):
    missing = output / "missing-resource.obj"
    missing.write_text(obj_path.read_text().replace("owned-pane.mtl", "absent.mtl"))
    textured = textured_fixture(output / "missing-image")
    textured.with_suffix(".png").unlink()
    calls = [
        lambda p=output / ("unsupported" + suffix): Asset.from_file(p)
        for suffix in (".fbx", ".gltf", ".glb")
    ]
    calls.extend((lambda: Asset.from_file(missing), lambda: Asset.from_file(textured)))
    for call in calls:
        try:
            call()
        except (ValueError, NotImplementedError, FileNotFoundError):
            pass
        else:
            raise AssertionError("Unsupported format or missing OBJ resource accepted")
    return len(calls)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    obj_path = fixtures(args.files)
    textured = textured_fixture(args.files / "textured")
    rows = [obj_control(obj_path), obj_control(textured)]
    record = revision() | dict(
        status="pass",
        cases=rows,
        channels=controls(),
        invalid=invalid_controls(args.files, obj_path),
        limits="Owned OBJ surfaces and direct native geometry only; authored UV/normals, material precision, encoded procedural image bytes, dependencies, repeated import, hierarchy/reflection/shear and native appearance channels checked. No compatibility requirement for other file formats or native/cache/frame admission.",
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(dict(status="pass", cases=len(rows), invalid=record["invalid"])))


if __name__ == "__main__":
    main()
