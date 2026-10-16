"""Real young-conifer operations registered on the unchanged shared DAG pilot."""

from pathlib import Path

from outshine_ref.vegetation import NeedledPlant, NeedlePattern, PlantPlan
from recipe_graph import Operator, Prepared
from recipe_graph_data import Budget, Files, Key, digest, plain
from recipe_plant_detail import generate_detail
from recipe_plant_sources import read_site
from recipe_plant_values import detail_emission, json_emission, read_json

FORM = Key("vegetation.pinus-sylvestris-young-form", "1")
SITE = Key("vegetation.pinus-sylvestris-young-site", "1")
CAPABILITIES = ("vegetation.needled-plants", "vegetation.planar-root-contact")
RECIPES = Path(__file__).resolve().parents[1] / "assets/recipes"


def wood(parameters, inputs):
    seed = inputs["seed"].value
    if type(seed) is not int or not 0 <= seed < 2**32:
        raise ValueError("Portable UInt32 instance seed required")
    levels = parameters.get("levels", ())
    count, total = 1, 0
    if not 1 <= len(levels) <= 4:
        raise ValueError("Pilot wood order budget exceeded before construction")
    for level in levels:
        branches = level.get("branches")
        if type(branches) is not int or not 1 <= branches <= 64:
            raise ValueError("Pilot branch work budget exceeded before construction")
        count *= branches
        total += count
    if total > 512:
        raise ValueError("Pilot axis work budget exceeded before construction")
    recipe = plain(parameters) | {"seed": seed}
    plan = PlantPlan.from_recipe(recipe)
    if len(plan.levels) > 4 or sum(plan.axis_counts) > 512:
        raise ValueError("Pilot wood-plan work bounds exceeded")
    return {"plan": json_emission(plan.recipe(), "PlantPlan recipe; no organ buffers")}


def patterns(parameters, inputs):
    if set(parameters) != {"patterns"} or not 1 <= len(parameters["patterns"]) <= 4:
        raise ValueError("Bounded explicit needle patterns required")
    rows = [
        NeedlePattern.from_recipe(plain(p)).recipe() if p is not None else None
        for p in parameters["patterns"]
    ]
    return {"patterns": json_emission(rows, "Per-order needle pattern recipes")}


def variants(parameters, inputs):
    from outshine_ref.vegetation import Needle

    if set(parameters) != {"needles"} or not 1 <= len(parameters["needles"]) <= 16:
        raise ValueError("Bounded explicit physical needle variants required")
    rows = [Needle(**plain(p)).recipe() for p in parameters["needles"]]
    return {"variants": json_emission(rows, "Physical needle parameter recipes")}


def plant(parameters, inputs):
    if parameters:
        raise ValueError("Needled composition has no implicit parameter overrides")
    recipe = {
        "version": "needled-plant-1",
        "wood": plain(read_json(inputs["wood"], "PlantPlan recipe; no organ buffers")),
        "patterns": plain(
            read_json(inputs["patterns"], "Per-order needle pattern recipes")
        ),
        "needles": plain(
            read_json(inputs["variants"], "Physical needle parameter recipes")
        ),
    }
    result = NeedledPlant.from_recipe(recipe)
    if sum(result.needle_counts) > 16384:
        raise ValueError("Pilot physical needle count exceeded before detail")
    return {
        "plant": json_emission(result.recipe(), "NeedledPlant recipe; deferred detail")
    }


def detail(parameters, inputs):
    recipe = read_json(inputs["plant"], "NeedledPlant recipe; deferred detail")
    value, _facts = generate_detail(recipe, parameters)
    result = detail_emission(value)
    return {"detail": result}


def placement(parameters, inputs):
    if parameters:
        raise ValueError("Root placement has no implicit physical parameters")
    recipe = read_json(inputs["plant"], "NeedledPlant recipe; deferred detail")
    site = read_site(inputs["site"])
    value = {
        "plant_recipe": recipe,
        "habitat_sha256": digest(site.habitat.recipe()),
        "site_id": site.site_id,
        "origin_m": site.origin.tolist(),
        "frame": site.frame.tolist(),
        "source_id": site.habitat.source_id,
        "provenance": site.provenance,
        "contact": "prepared planar root only; no DEM/crown collision certificate",
    }
    result = json_emission(value, "Placed root transform plus unchanged plant recipe")
    from recipe_graph import Emission

    return {
        "placement": Emission(
            result.value,
            result.nbytes,
            dict(inputs["site"].metadata)
            | {
                "representation": "Root placement; form-local geometry stays separate",
                "content_sha256": digest(value),
                "accounting": result.metadata["accounting"],
            },
        )
    }


def operators():
    definitions = (
        ("wood-plan", {"seed": "seed"}, {"plan": "plant-plan"}, wood),
        ("needle-patterns", {}, {"patterns": "needle-patterns"}, patterns),
        ("needle-variants", {}, {"variants": "needle-variants"}, variants),
        (
            "needled-plant",
            {
                "wood": "plant-plan",
                "patterns": "needle-patterns",
                "variants": "needle-variants",
            },
            {"plant": "needled-plant"},
            plant,
        ),
        (
            "needled-detail",
            {"plant": "needled-plant"},
            {"detail": "needled-detail"},
            detail,
        ),
        (
            "place-plant",
            {"plant": "needled-plant", "site": "habitat-site"},
            {"placement": "plant-placement"},
            placement,
        ),
    )
    return tuple(
        Operator(
            Key("vegetation." + key, "1"),
            {k: "vegetation." + v + "/1" for k, v in inputs.items()},
            {k: "vegetation." + v + "/1" for k, v in outputs.items()},
            function,
        )
        for key, inputs, outputs, function in definitions
    )


def prepared(key=FORM, *, loader=None, budget=None):
    if budget is None:
        budget = Budget()
    if loader is None:
        loader = Files(
            {
                FORM: RECIPES / "vegetation-pinus-young-form-v1.json",
                SITE: RECIPES / "vegetation-pinus-young-site-v1.json",
            }
        )
    return Prepared(key, loader, operators(), capabilities=CAPABILITIES, budget=budget)
