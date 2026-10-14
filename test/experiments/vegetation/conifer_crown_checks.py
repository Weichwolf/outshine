"""Independent whole-packet IDs, replay, poses and matching near shoots."""

import json

import numpy as np
from conifer_crown_products import OUTPUT
from conifer_crown_recipes import crown_conifers
from outshine_ref.vegetation import NeedledPlant


def check_species(species):
    directory = OUTPUT / species.key
    index = json.loads((directory / "whole-index.json").read_text())
    plan = NeedledPlant.from_recipe(
        json.loads((directory / "whole-plan.json").read_text())
    )
    assert plan.identity == index["identity"] == species.plant().identity
    summary = plan.summary()
    if summary["needles"] > 100000:
        assert plan.tables(100000, 100000) is None and not plan._groups
    arrays = {k: [] for k in ("origins", "frames", "ids", "scales")}
    maximum = 0
    for records in index["needle_packets"]:
        for r in records:
            with np.load(directory / r["placements"]) as data:
                assert len(data["ids"]) == r["instances"] <= 16384
                maximum = max(maximum, r["instances"])
                for k in arrays:
                    arrays[k].append(data[k])
    whole = {k: np.concatenate(v) for k, v in arrays.items()}
    order = np.argsort(whole["ids"])
    whole = {k: v[order] for k, v in whole.items()}
    np.testing.assert_array_equal(
        whole["ids"], np.arange(summary["needles"], dtype=np.uint64)
    )
    selected = (
        (0,) * 3,
        tuple(level.branches - 1 for level in species.levels[1:]),
        tuple(level.branches // 2 for level in species.levels[1:]),
    )
    for path in selected:
        g = plan.foliage(path, 100000, retain=False)
        for k in whole:
            np.testing.assert_array_equal(whole[k][g["ids"]], g[k])
    scene = json.loads((directory / "shoot.json").read_text())
    for mode in ("whole", "shoot"):
        products = json.loads((directory / f"{mode}.json").read_text())["products"]
        assert all(p["surface"] in ("wood", "leaf") for p in products)
    count = 0
    for product in scene["products"]:
        if "needle_material" not in product:
            continue
        with np.load(directory / product["placements"]) as near:
            for k in whole:
                np.testing.assert_array_equal(whole[k][near["ids"]], near[k])
            count += len(near["ids"])
    assert count == len(plan.foliage(selected[-1], 100000, retain=False)["ids"])
    assert not plan._groups
    return {
        "species": species.key,
        "needles": len(whole["ids"]),
        "max_packet_rows": maximum,
        "matched_near_needles": count,
        "retained_details": len(plan._groups),
    }


def main():
    records = [check_species(s) for s in crown_conifers()]
    result = {"species": records, "tile": check_tile()}
    (OUTPUT / "checks.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


def check_tile():
    directory = OUTPUT / "tile"
    scene = json.loads((directory / "tile.json").read_text())
    plants = json.loads((directory / scene["plant_recipe"]).read_text())["plants"]
    maximum, packets, rows = 0.0, 0, 0
    for record in scene["products"]:
        if "plant_id" not in record:
            continue
        plant = plants[record["plant_id"]]
        source = (directory / plant["recipe"]).parent
        name = record["placements"].removeprefix("tile-")
        frame, origin = np.array(plant["frame"]), np.array(plant["origin_m"])
        with (
            np.load(directory / record["placements"]) as world,
            np.load(source / name) as local,
        ):
            recovered = (world["origins"] - origin) @ frame
            maximum = max(maximum, float(np.max(abs(recovered - local["origins"]))))
            np.testing.assert_allclose(recovered, local["origins"], atol=5e-14, rtol=0)
            recovered_frames = np.einsum("ij,njk->nik", frame.T, world["frames"])
            np.testing.assert_allclose(
                recovered_frames, local["frames"], atol=5e-14, rtol=0
            )
            for key in ("ids", "scales"):
                np.testing.assert_array_equal(world[key], local[key])
            rows += len(world["ids"])
            packets += 1
    return {"packets": packets, "poses": rows, "inverse_position_error_max_m": maximum}


if __name__ == "__main__":
    main()
