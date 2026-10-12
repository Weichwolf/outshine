"""Length-adaptive arrangements rendered through complete graph products."""

import json
import subprocess
from dataclasses import replace

import numpy as np
from blade_surface import Blade
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from graph_products import graph_products
from placement_patterns import DistancePattern
from profile_curve import Profile


def demonstration(mode, seed=17):
    t = np.linspace(0, 1, 4)
    stem_placement = Placement(
        pitch=0, pitch_variance=0, scale_variance=0, azimuth_variance=0
    )
    pattern = DistancePattern(
        mode=mode,
        spacing=Profile(
            np.array([0.12, 0.18, 0.24, 0.12])
            if mode == "interval"
            else np.full(4, 0.08)
        ),
        whorl_size=3 if mode == "interval" else 2,
        divergence=0.35 if mode == "interval" else np.pi / 2,
        spread_m=0.015 if mode == "interval" else 0,
    )
    parents = (1, 2, 3)
    leaf = Blade(length=0.17, width=0.065, widest=0.4, bend=0.6, fold=0.18)
    stems = tuple(
        Generator(
            i + 1,
            f"Stamm {L}",
            "axis",
            (0,),
            TubeShape(L, 0.10, Profile(0.012 * (1 - 0.55 * t))),
            stem_placement,
        )
        for i, L in enumerate((0.55, 0.95, 1.35))
    )
    nodes = (Generator(0, "Wurzel", "root", ()),) + stems
    if mode == "interval":
        twigs = TubeShape(0.34, 0.38, Profile(0.003 * (1 - 0.65 * t)))
        nodes += (
            Generator(
                4,
                "Astquirle",
                "axis",
                parents,
                twigs,
                Placement(first=0.18, last=0.92, pitch=1.05, pattern=pattern),
            ),
        )
        parents = (4,)
        pattern = DistancePattern(spacing=Profile(np.full(4, 0.045)), divergence=np.pi)
        leaf = replace(leaf, length=0.105, width=0.038)
    nodes += (
        Generator(
            5,
            "Blattfolge",
            "blade",
            parents,
            leaf,
            Placement(
                first=0.10, last=0.93, pitch=1.0, pitch_variance=0.08, pattern=pattern
            ),
        ),
    )
    return GeneratorGraph(seed, nodes)


def make_scenes(output):
    reports = []
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for mode in ("phyllotaxy", "interval"):
        graph = demonstration(mode)
        directory = output / mode
        near = graph_products(graph, directory, "near")
        coarse = graph_products(graph, directory, "coarse", max_new_poses=0)
        assert near and coarse
        placement_files = {
            p["placements"] for p in near["products"] + coarse["products"]
        }
        for filename in placement_files:
            path = directory / filename
            with np.load(path) as stored:
                values = {key: stored[key] for key in stored.files}
            offsets = values["path_offsets"]
            roots = np.array(
                [values["path_pairs"][start + 1, 0] for start in offsets[:-1]]
            ).astype(np.int64)
            values["origins"][:, 0] += (roots - 2) * 0.85
            assert np.isfinite(values["origins"]).all()
            assert abs(values["origins"][:, 0]).max() < 2
            np.savez_compressed(path, **values)
        for name, product, camera, target in (
            (f"placement-{mode}", near, (2.7, -4.7, 2.15), (0, 0, 0.70)),
            (f"placement-{mode}-coarse", coarse, (2.7, -4.7, 2.15), (0, 0, 0.70)),
        ):
            recipe = product | {
                "name": name,
                "camera": camera,
                "target": target,
                "fov_y": 38,
                "ground_size_m": 10,
                "resolution": [2560, 1440],
                "samples": 32,
                "source_commit": commit,
                "source_modified": True,
                "presentation_root_offsets_m": [-0.85, 0, 0.85],
            }
            (directory / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
        reports.append(
            {
                "mode": mode,
                "summary": graph.summary(),
                "fine_prototype_triangles": near["prototype_triangles"],
                "coarse_prototype_triangles": coarse["prototype_triangles"],
            }
        )
    return reports
