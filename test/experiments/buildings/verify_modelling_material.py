"""Check documented shape/geometry material precedence, colours, resets and source ownership."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np

from evidence import revision
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.material_schema import (
    srgb_to_linear,
    linear_to_srgb,
    DEFAULTS,
)
from modelling_uv_cases import adjacent, courtyard
from outshine_ref.buildings.modelling.split_sizes import Part, Floating
from modelling_asset_cases import COLOUR, textured_fixture
from outshine_ref.buildings.modelling.scope import Scope
from outshine_ref.buildings.modelling.shape import Shape


def values(shape, source="used", mode="changed"):
    return dict(shape.get_material(source, mode))


def controls():
    source = adjacent().with_metadata(
        material_defs={
            7: Material(
                "left",
                {"color.r": 0.2, "color.g": 0.3, "color.b": 0.4, "roughness": 0.6},
            ),
            8: Material("right", {"color.r": 0.8, "metallic": 1.0}),
        }
    )
    original = source.geometry
    altered = source.color(-1.0, 0.5, 2.0, 0.25)
    assert altered.geometry is original and altered.scope == source.scope
    assert values(altered) == dict(
        name="left",
        roughness=format(0.6, ".17g"),
        **{"color.r": "0", "color.g": "0.5", "color.b": "1", "opacity": "0.25"},
    )
    assert (
        values(altered, "geometry") == values(source, "geometry")
        and values(source, "shape") == {}
    )
    assert values(altered.color("#204060"), "shape")["opacity"] == "0.25"
    assert float(values(source.color("#20406080"), "shape")["opacity"]) == 128 / 255.0
    reset = altered.reset_material()
    assert reset.geometry is original and values(reset) == values(source)
    geo = altered.reset_material("geometry")
    assert geo.material == altered.material and values(geo, "geometry") == {}
    both = altered.reset_material("all")
    assert values(both) == {} and both.geometry.unit_vertices is original.unit_vertices
    copied = source.set_material(altered.get_material("used", "all"))
    assert values(copied, "used", "all") == values(altered, "used", "all")
    explicit = source.set_material(
        ["color.r", "1.0", "doubleSided", "false", "opacitymap.mode", "mask"]
    )
    assert values(explicit, "shape") == {
        "color.r": "1",
        "doubleSided": "false",
        "opacitymap.mode": "mask",
    }
    coloured = source.color(0.7, 0.6, 0.5)
    prepared = Asset.from_shape(coloured)
    placed = source.color(0.1, 0.2, 0.3).insert(prepared)
    assert placed.is_instanced() and values(placed, "shape") == values(
        source.color(0.1, 0.2, 0.3), "shape"
    )
    assert float(values(placed, "geometry")["color.r"]) == 0.7
    for child in coloured.comp("f"):
        assert child.material == coloured.material
    for child in coloured.split("x", Part(Floating(1)), Part(Floating(1))):
        assert child.material == coloured.material
    for child in coloured.normalize_uv(0, "uv", "separatePerFace").comp("m"):
        assert child.material == coloured.material
    extruded = courtyard().color(0.4, 0.5, 0.6).extrude(3.0)
    assert extruded.material == courtyard().color(0.4, 0.5, 0.6).material
    no_uv = source.delete_uv(0).color(0.1, 0.2, 0.3)
    assert "uv" not in no_uv.emit()
    linear = source.with_metadata(
        material_defs={
            7: Material(
                "linear-source",
                {
                    "shader": "CityEnginePBRShader",
                    "pbrMetallicRoughness.baseColorFactor": (
                        0.21404114048223255,
                        0.0,
                        1.0,
                        0.4,
                    ),
                },
            ),
            8: Material(),
        }
    )
    assert (
        np.isclose(float(values(linear)["color.r"]), 0.5)
        and float(values(linear)["opacity"]) == 0.4
    )
    np.testing.assert_allclose(
        srgb_to_linear([0.0, 0.04045, 0.5, 1.0]),
        [0.0, 0.0031308049535603713, 0.21404114048223255, 1.0],
        rtol=0,
        atol=1e-15,
    )
    np.testing.assert_allclose(
        linear_to_srgb(srgb_to_linear([0.0, 0.03, 0.5, 1.0])),
        [0.0, 0.03, 0.5, 1.0],
        rtol=0,
        atol=1e-14,
    )
    assert (
        DEFAULTS["shader"] == "CityEngineShader"
        and DEFAULTS["roughness"] == 1.0
        and DEFAULTS["doubleSided"] is True
    )
    return dict(
        shape_geometry_precedence=True,
        explicit_default_roundtrip=True,
        resets_three_targets=True,
        color_numeric_clamps=True,
        hex_rgb_rgba=True,
        srgb_linear_bridge=True,
        scope_comp_split_extrude_inheritance=True,
        insert_override_and_preparation=True,
        geometry_arrays_shared=True,
    )


def invalid_controls():
    source = adjacent()
    calls = [
        lambda: source.color("#fff"),
        lambda: source.color(1, 2),
        lambda: source.color(0, 0, float("inf")),
        lambda: source.set_material(["color.r"]),
        lambda: source.set_material({"material.color.r": 0.5}),
        lambda: source.set_material({"unknown": 1}),
        lambda: source.set_material({"doubleSided": 1}),
        lambda: source.set_material({"opacitymap.mode": "bad"}),
        lambda: source.reset_material("bad"),
        lambda: source.get_material("bad"),
        lambda: replace(source, material={}),
        lambda: source.set_material({"roughness": float("nan")}),
        lambda: values(
            source.with_metadata(
                material_defs={7: Material("bad", {"kd": COLOUR[:3], "d": (1, 2)})}
            )
        ),
        lambda: values(
            source.with_metadata(
                material_defs={
                    7: Material("bad", {"kd": COLOUR[:3], "d": (float("nan"),)})
                }
            )
        ),
    ]
    for call in calls:
        try:
            call()
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid material operation accepted")
    return len(calls)


def imported_ownership(output):
    asset = Asset.from_file(textured_fixture(output))
    original = Shape(
        asset.geometry,
        Scope(position=asset.minimum, size=asset.geometry.reference_size),
        asset=asset,
    )
    coloured = original.color(0.2, 0.3, 0.4)
    prepared = Asset.from_shape(coloured)
    assert (
        prepared.resources is asset.resources
        and prepared.import_info is asset.import_info
    )
    placed = original.insert(prepared, "keepSizeAndPosition")
    assert float(values(placed)["color.r"]) == 0.2
    raw = dict(dict(prepared.geometry.material_defs)[1].attributes)
    assert "kd" in raw and raw["color.r"] == 0.2
    assert raw["d"] == dict(dict(asset.geometry.material_defs)[1].attributes)["d"]
    assert float(values(original)["opacity"]) == COLOUR[3]
    assert float(values(placed)["opacity"]) == COLOUR[3]
    assert len(asset.resources.images) == 1 and asset.resources.images[0].encoded
    assert asset.geometry.material_defs == original.geometry.material_defs and values(
        original
    ) == values(coloured.reset_material())
    return dict(
        encoded_images_retained=True,
        raw_material_values_retained=True,
        original_asset_unmodified=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    record = revision() | dict(
        status="pass",
        controls=controls(),
        imported=imported_ownership(args.output.parent / "material-import-controls"),
        invalid=invalid_controls(),
        limits="Documented core scalar/string/default material semantics; complete texture-resource remapping, readMaterial/cgamat, exact SDK value formatting, shader equivalence and isolated CGA material rules remain open.",
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps(
            dict(
                status="pass",
                controls=len(record["controls"]),
                invalid=record["invalid"],
            )
        )
    )


if __name__ == "__main__":
    main()
