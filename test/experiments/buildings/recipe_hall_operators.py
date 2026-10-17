"""Real source/roof/facade/access/CSG adapters for the isolated shared recipe pilot."""

from collections.abc import Mapping
from dataclasses import dataclass, is_dataclass
from dataclasses import fields as dataclass_fields
from pathlib import Path
from types import MappingProxyType

import numpy as np
import shapely
from facade_axes import layout
from facade_entrances import EntranceDetails
from industrial_hall_plan import metadata as hall_metadata
from industrial_hall_plan import plan as hall_plan
from industrial_hall_shell import structure
from industrial_roof_choice import KINDS
from industrial_roof_choice import metadata as roof_metadata
from outshine_ref.buildings.modelling import heightfield_envelope
from outshine_ref.contracts import LocalFrame, geometry, immutable
from provider_contract import attributes
from recipe_graph import Emission, Operator, Prepared, external
from recipe_graph_data import Files, Key, canonical, fields
from recipes import choose as facade_recipe
from tile_roof_envelopes import choose, roof_surface

ROOT = Key("buildings.industrial-hall", "1")
ROOF = Key("buildings.industrial-envelope", "1")
CAPABILITIES = ("buildings.industrial-roof", "buildings.hall-csg")
TYPES = {
    name: "buildings." + name + "/1"
    for name in (
        "source",
        "site",
        "roof-choice",
        "roof-envelope",
        "hall-programme",
        "hall-shell",
    )
}


@dataclass(frozen=True, slots=True)
class Programme:
    plan: Mapping
    facades: tuple
    entrance: Mapping


@dataclass(frozen=True, slots=True)
class HallShell:
    body: Mapping
    shell: object


def encoded(row):
    return {k: v for k, v in row.items() if k != "polygon"} | {
        "footprint_wkb": row["polygon"].wkb_hex
    }


def inputs_for(row, neighbors=(), *, frame_id=None):
    common = {
        "frame_id": frame_id or row["id"].rsplit("/building/", 1)[0],
        "axes": "east,north,up",
        "units": "m",
        "height_datum": "flat-local-ground0",
    }
    LocalFrame(common["frame_id"])
    geometry(row["polygon"], polygonal=True)
    if len({r["id"] for r in neighbors}) != len(neighbors):
        raise ValueError("Neighbor sources require unique identities")
    neighbors = sorted(neighbors, key=lambda r: r["id"])
    source = encoded(row)
    site = [encoded(r) for r in neighbors]

    def snapshot(value, original):
        geometry(original["polygon"], polygonal=True)
        return MappingProxyType(dict(immutable(value), polygon=original["polygon"]))

    return {
        "source": external(
            TYPES["source"],
            snapshot(source, row),
            source,
            len(canonical(source)),
            common | {"representation": "provider-footprint-wkb-float64"},
        ),
        "site": external(
            TYPES["site"],
            tuple(
                snapshot(value, original)
                for value, original in zip(site, neighbors, strict=True)
            ),
            site,
            len(canonical(site)),
            common | {"representation": "provider-neighbor-wkb-float64"},
        ),
    }


def compatible(*products):
    expected = products[0].metadata
    for product in products:
        for key in ("frame_id", "axes", "units", "height_datum"):
            if product.metadata.get(key) != expected.get(key):
                raise ValueError("Hall input frames/units/height datums disagree")
        if (
            product.metadata.get("axes") != "east,north,up"
            or product.metadata.get("units") != "m"
            or product.metadata.get("height_datum") != "flat-local-ground0"
        ):
            raise ValueError("Hall pilot requires flat local ENU metres")


def spatial(source, representation):
    return dict(source.metadata) | {
        "representation": representation,
        "accounting": "Distinct ndarray storage plus encoded logical descriptors; Python/Shapely/native overhead excluded",
    }


def sealed(value):
    if isinstance(value, Mapping):
        return MappingProxyType({key: sealed(item) for key, item in value.items()})
    if isinstance(value, (list, tuple)):
        return tuple(sealed(item) for item in value)
    if isinstance(value, np.ndarray) and value.flags.writeable:
        result = value.copy()
        result.flags.writeable = False
        return result
    return value


def array_bytes(value):
    seen = set()

    def visit(item):
        if isinstance(item, np.ndarray):
            base = item
            while isinstance(base.base, np.ndarray):
                base = base.base
            if id(base) in seen:
                return 0
            seen.add(id(base))
            return base.nbytes
        if isinstance(item, Mapping):
            return sum(visit(v) for v in item.values())
        if isinstance(item, (list, tuple)):
            return sum(visit(v) for v in item)
        if is_dataclass(item):
            return sum(visit(getattr(item, f.name)) for f in dataclass_fields(item))
        return 0

    return int(visit(value))


def choice_operator(parameters, inputs):
    fields(parameters, ())
    source = inputs["source"]
    compatible(source)
    row = source.value
    params = attributes(row["properties"])
    choice = choose(row, params)
    if choice.kind not in KINDS:
        raise ValueError(
            "Source is not eligible for the existing industrial roof recipe"
        )
    value = choice
    return {
        "choice": Emission(
            value,
            len(canonical(roof_metadata(row, choice))),
            spatial(source, "roof-choice-metres-binary64"),
        )
    }


def wall_material(parameters):
    fields(parameters, ("wall_material",))
    material = parameters["wall_material"]
    if type(material) is not int or not 0 <= material <= 65535:
        raise ValueError("Wall material requires uint16")
    return material


def representation(mesh, roots, *, envelope=False):
    expected = {
        "vertices": "float64",
        "normals": "float64",
        "uv": "float64",
        "materials": "uint16",
        "faces": "uint32" if envelope else "int32",
    }
    expected |= (
        {"parts": "int32", "groups": "uint32", "face_tag_mask": "uint8"}
        if envelope
        else {"facade_extent": "float64"}
    )
    if set(mesh) != set(expected) or any(
        not isinstance(mesh[k], np.ndarray) or mesh[k].dtype != np.dtype(dtype)
        for k, dtype in expected.items()
    ):
        raise ValueError(
            "Building product has an uncontracted actual array representation"
        )
    if not isinstance(roots, np.ndarray) or roots.dtype != np.dtype(
        "uint64" if envelope else "int64"
    ):
        raise ValueError(
            "Building product has an uncontracted topology-root representation"
        )
    vertices, faces = len(mesh["vertices"]), len(mesh["faces"])
    shapes = {key: (vertices, 3) for key in ("vertices", "normals")}
    shapes |= {"uv": (vertices, 2), "faces": (faces, 3), "materials": (faces,)}
    shapes |= (
        {key: (faces,) for key in ("parts", "groups", "face_tag_mask")}
        if envelope
        else {"facade_extent": (vertices, 3)}
    )
    if roots.shape != (vertices,) or any(
        mesh[k].shape != shape for k, shape in shapes.items()
    ):
        raise ValueError("Building product stream dimensions disagree")
    for key in ("vertices", "normals", "uv"):
        if not np.isfinite(mesh[key]).all():
            raise ValueError("Building product contains nonfinite arrays")


def envelope_operator(parameters, inputs):
    material = wall_material(parameters)
    source, choice = inputs["source"], inputs["choice"]
    compatible(source, choice)
    row = source.value
    p = attributes(row["properties"])
    top = roof_surface(row, choice.value)[0]
    value = heightfield_envelope(
        top, p["minHeight"], wall_material=material, base_material=material
    )
    representation(value.mesh, value.vertex_roots, envelope=True)
    return {
        "envelope": Emission(
            value,
            array_bytes(value) + len(value.footprint_wkb),
            spatial(source, "heightfield-envelope-float64-topology-roots"),
        )
    }


def programme_operator(parameters, inputs):
    fields(parameters, ())
    source, choice, site = inputs["source"], inputs["choice"], inputs["site"]
    compatible(source, choice, site)
    row = source.value
    p = attributes(row["properties"])
    roof = choice.value
    recipe = facade_recipe(
        row["properties"],
        row.get("context", {}),
        row["polygon"],
        p["buildingHeight"],
        p["minHeight"],
    )
    if recipe["family"] != "industrial":
        raise ValueError("No industrial facade recipe for this source")
    rhythm = recipe["parameters"]
    info = {"eave_m": roof.eave_m, "roof_section": roof_metadata(row, roof)}
    facades = layout(
        row,
        p | info,
        (rhythm["width"], rhythm["height"]),
        rhythm["axis_m"],
        rhythm["floor_m"],
    )
    rows = [row, *site.value]
    entrances = EntranceDetails(rows, shapely.STRtree([r["polygon"] for r in rows]))
    entry = entrances.plan(row, p, facades, "industrial")
    value = hall_plan(row, p, info, facades, entry, entrances.clear)
    if value is None:
        raise ValueError("Source has no eligible single hall programme")
    value = Programme(sealed(value), sealed(facades), sealed(entrances.planned[-1]))
    return {
        "programme": Emission(
            value,
            len(canonical(hall_metadata(value.plan)))
            + len(canonical(facades))
            + len(canonical(value.entrance)),
            spatial(source, "source-bound-hall-programme-metres-binary64"),
        )
    }


def shell_operator(parameters, inputs):
    material = wall_material(parameters)
    envelope, programme = inputs["envelope"], inputs["programme"]
    compatible(envelope, programme)
    value = programme.value
    body, shell = structure(value.plan, envelope.value, material)
    representation(body, shell.vertex_roots)
    result = HallShell(sealed(body), shell)
    return {
        "shell": Emission(
            result,
            array_bytes(result),
            spatial(envelope, "cut-hall-shell-float64-topology-roots"),
        )
    }


def operators():
    return [
        Operator(
            Key("buildings.roof-choice", "1"),
            {"source": TYPES["source"]},
            {"choice": TYPES["roof-choice"]},
            choice_operator,
        ),
        Operator(
            Key("buildings.close-envelope", "1"),
            {"source": TYPES["source"], "choice": TYPES["roof-choice"]},
            {"envelope": TYPES["roof-envelope"]},
            envelope_operator,
        ),
        Operator(
            Key("buildings.hall-programme", "1"),
            {
                "source": TYPES["source"],
                "choice": TYPES["roof-choice"],
                "site": TYPES["site"],
            },
            {"programme": TYPES["hall-programme"]},
            programme_operator,
        ),
        Operator(
            Key("buildings.cut-hall", "1"),
            {"envelope": TYPES["roof-envelope"], "programme": TYPES["hall-programme"]},
            {"shell": TYPES["hall-shell"]},
            shell_operator,
        ),
    ]


def prepare(*, budget=None, loader=None):
    folder = Path(__file__).parents[1] / "assets" / "recipes"
    files = Files(
        {
            ROOF: folder / "buildings-industrial-envelope-v1.json",
            ROOT: folder / "buildings-industrial-hall-v1.json",
        }
    )
    return Prepared(ROOT, loader or files, operators(), CAPABILITIES, budget)
