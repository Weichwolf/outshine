"""Independent opaque-mask correlation and complete bounded face partitions."""

import argparse
import json
from pathlib import Path
from unittest.mock import patch as mock_patch

import numpy as np
from needle_composition import composite
from needle_group_reprojection import reproject
from needle_group_source import source_geometry
from needle_spatial_cache import load_product
from needle_spatial_planes import plane_patches
from needle_spatial_product import trim_view
from outshine_ref.vegetation import Needle, NeedledPlant


def image(alpha, rgb, z):
    alpha = np.asarray(alpha, float)[..., None]
    return {
        "rgba": np.concatenate((alpha * rgb, alpha), axis=-1),
        "depth": alpha * z,
        "normal": np.broadcast_to((0, 0, 1), (*alpha.shape[:2], 3)) * alpha,
        "pixel_area_m2": 1,
    }


def mask_correlation():
    front = image(((1, 0), (1, 0)), (1, 0, 0), 2)
    disjoint = image(((0, 1), (0, 1)), (0, 0, 1), 1)
    coincident = image(((1, 0), (1, 0)), (0, 0, 1), 1)
    a = composite((front, disjoint), 2)
    b = composite((front, coincident), 2)
    np.testing.assert_array_equal(a["rgba"], [[(0.5, 0, 0.5, 1)]])
    np.testing.assert_array_equal(b["rgba"], [[(0.5, 0, 0, 0.5)]])
    np.testing.assert_array_equal(a["depth"], [[[1.5]]])
    np.testing.assert_array_equal(b["depth"], [[[1]]])
    for layers in ((disjoint, front), (coincident, front)):
        reverse = composite(layers, 2)
        expected = a if layers[0] is disjoint else b
        np.testing.assert_array_equal(reverse["rgba"], expected["rgba"])


def partition_checks(source, epsilon=0.0005):
    patches, remainder = plane_patches(source, epsilon)
    all_faces = np.concatenate([*(p["faces"] for p in patches), remainder])
    np.testing.assert_array_equal(np.sort(all_faces), np.arange(len(source["faces"])))
    for patch in patches:
        vertices = source["vertices"][source["faces"][patch["faces"]]]
        displacement = abs(vertices @ patch["normal"] - patch["offset_m"]).max()
        assert displacement <= epsilon * (1 + 1e-10)
        assert displacement == patch["displacement_m"]
    again, rest = plane_patches(source, epsilon)
    np.testing.assert_array_equal(rest, remainder)
    for first, second in zip(patches, again, strict=True):
        for name in first:
            np.testing.assert_array_equal(first[name], second[name])
    zero, all_original = plane_patches(source, epsilon, max_charts=0)
    assert zero == []
    np.testing.assert_array_equal(all_original, np.arange(len(source["faces"])))
    return len(patches), len(remainder)


def trim_checks():
    rgba = np.zeros((16, 16, 4))
    rgba[4:12, 6:10] = (0.2, 0.4, 0.6, 1)
    view = {
        "frame": np.eye(3),
        "lower": np.array((-1, -1)),
        "upper": np.array((1, 1)),
        "rgba": rgba,
        "normal": rgba[..., 3, None] * (0, 0, 1),
        "depth": rgba[..., 3, None] * 0.25,
        "roughness_fourth": rgba[..., 3, None] * 0.8**4,
        "pixel_area_m2": 1 / 64,
    }
    trimmed = trim_view(view)
    assert trimmed["rgba"].shape[:2] == (10, 6)
    source = {"vertices": np.array(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)))}
    for direction in ((0, 0, 1), (1, 0, 1), (0, 1, 1)):
        a, b = (reproject(v, source, direction) for v in (view, trimmed))
        for field in ("rgba", "normal", "depth"):
            np.testing.assert_allclose(a[field], b[field], atol=1e-6)


def cache_checks():
    from needle_spatial_experiment import HERE, OUTPUT, sha

    records = json.loads((OUTPUT / "report.json").read_text())
    with (
        mock_patch.object(Needle, "mesh", side_effect=AssertionError("cached meshing")),
        mock_patch.object(
            NeedledPlant,
            "foliage",
            side_effect=AssertionError("cached organ placement"),
        ),
    ):
        for record in records:
            for name, expected in record["implementation_sha256"].items():
                assert sha(HERE / name) == expected, name
            for variant in record["variants"]:
                path = Path(variant["cache_path"])
                assert sha(path) == variant["cache_sha256"]
                source, products, remainder = load_product(path, record["identity"])
                assert (
                    source["identity"] == record["identity"]
                    and source["axis_path"] == record["axis_path"]
                )
                all_faces = np.concatenate(
                    [*(p["patch"]["faces"] for p in products), remainder]
                )
                np.testing.assert_array_equal(
                    np.sort(all_faces), np.arange(len(source["faces"]))
                )
                for value in source.values():
                    if isinstance(value, np.ndarray):
                        assert not value.flags.writeable
                try:
                    load_product(path, "wrong identity")
                except ValueError:
                    pass
                else:
                    raise AssertionError("wrong cached identity accepted")
            print(
                record["species"],
                "complete two-sided product decode checked",
                flush=True,
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", action="store_true")
    args = parser.parse_args()

    mask_correlation()
    trim_checks()
    if args.cache:
        cache_checks()
        return
    for species in ("picea-abies", "pinus-sylvestris", "pinus-cembra", "larix-decidua"):
        source = source_geometry(
            Path("build/experiments/vegetation/conifer-crowns") / species
        )
        print(species, partition_checks(source), flush=True)
    print("subsample correlation, complete partitions and object-space bounds passed")


if __name__ == "__main__":
    main()
