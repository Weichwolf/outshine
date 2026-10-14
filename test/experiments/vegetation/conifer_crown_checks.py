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
    (OUTPUT / "checks.json").write_text(json.dumps(records, indent=2) + "\n")
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
