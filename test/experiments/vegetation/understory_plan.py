"""Own Convallaria-majalis study after the local SIGGRAPH1988 architecture reference."""

from dataclasses import asdict, dataclass

import numpy as np
from biome_models import Assembly
from outshine_ref.vegetation import Bellflower, Blade
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.shoot_plan import normal_frame

UPRIGHT = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)), float)


@dataclass(frozen=True)
class LilyStudy:
    seed: int = 17
    flowers: int = 9

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or type(self.flowers) is not int
            or not 3 <= self.flowers <= 16
        ):
            raise ValueError("uint64 seed and three to16 flowers required")

    def recipe(self):
        rng = np.random.Generator(np.random.PCG64(self.seed))
        leaves = []
        for i in range(2):
            blade = Blade(
                length=rng.uniform(0.20, 0.25),
                width=rng.uniform(0.045, 0.065),
                widest=0.52,
                lobes=0,
                lobe_depth=0,
                base_fill=0.04,
                bend=rng.uniform(-0.25, 0.65),
                fold=rng.uniform(0.12, 0.24),
                twist=rng.uniform(-0.12, 0.12),
                petiole=0.018,
            )
            azimuth, lean = i * np.pi + rng.uniform(-0.18, 0.18), rng.uniform(0.55, 0.8)
            direction = np.array(
                (
                    np.sin(lean) * np.cos(azimuth),
                    np.sin(lean) * np.sin(azimuth),
                    np.cos(lean),
                )
            )
            frame = normal_frame(direction, (0, -1, 0))
            node = np.array((0, 0, 0.01 + i * 0.009))
            leaves.append(
                {
                    "id": [0, i],
                    "node": node.tolist(),
                    "origin": (node + frame[:, 1] * blade.petiole).tolist(),
                    "frame": frame.tolist(),
                    "blade": asdict(blade),
                }
            )
        length, bend = rng.uniform(0.23, 0.27), rng.uniform(0.5, 0.7)
        positions = (
            arc_frame(length, bend, 0, np.linspace(0.38, 0.96, self.flowers))[0]
            @ UPRIGHT.T
        )
        stalks = []
        for i, node in enumerate(positions):
            scale = 1 - 0.42 * i / (self.flowers - 1)
            end = node + np.array((-0.012 * scale, 0.015 * scale, -0.010 * scale))
            stalks.append(
                {
                    "id": [2, i],
                    "node": node.tolist(),
                    "end": end.tolist(),
                    "scale": scale,
                }
            )
        return {
            "format": "convallaria-study-1",
            "seed": self.seed,
            "numpy": np.__version__,
            "leaves": leaves,
            "stem_length_m": length,
            "stem_bend_rad": bend,
            "stalks": stalks,
            "scope": "Own authored dimensions/flowers after SIGGRAPH1988 Fig9; not botanical species acceptance.",
        }

    def organ_ids(self):
        return (
            (0, 0),
            (0, 1),
            (1, 0),
            *((2, i) for i in range(self.flowers)),
            *((3, i) for i in range(self.flowers)),
        )

    def mesh(
        self,
        leaf_segments=12,
        bell_segments=6,
        bell_sides=24,
        selected=None,
        max_organs=64,
    ):
        ids = self.organ_ids()
        requested = set(ids) if selected is None else set(selected)
        if type(max_organs) is not int or max_organs < 0 or not requested <= set(ids):
            raise ValueError(
                "known organ identities and nonnegative organ budget required"
            )
        if len(requested) > max_organs:
            return None
        return lily_mesh(
            self.recipe(),
            leaf_segments,
            bell_segments,
            bell_sides,
            requested,
            max_organs,
        )


def lily_mesh(
    recipe,
    leaf_segments=12,
    bell_segments=6,
    bell_sides=24,
    selected=None,
    max_organs=64,
):
    if (
        recipe["format"] != "convallaria-study-1"
        or not 3 <= len(recipe["stalks"]) <= 16
        or len(recipe["leaves"]) != 2
    ):
        raise ValueError("complete bounded lily study recipe required")
    ids = (
        (0, 0),
        (0, 1),
        (1, 0),
        *((2, i) for i in range(len(recipe["stalks"]))),
        *((3, i) for i in range(len(recipe["stalks"]))),
    )
    selected = set(ids) if selected is None else set(selected)
    if not selected <= set(ids) or type(max_organs) is not int or max_organs < 0:
        raise ValueError("known organ identities and nonnegative organ budget required")
    if len(selected) > max_organs:
        return None
    if not selected:
        raise ValueError("nonempty organ request required")
    assembly = Assembly()
    ranges = []

    def add(identity, mesh, origin, frame, surface):
        begin = assembly.vertices
        assembly.add(mesh, np.asarray(origin)[None], np.asarray(frame)[None], surface)
        ranges.append({"id": identity, "vertex_range": [begin, assembly.vertices]})

    for leaf in recipe["leaves"]:
        identity = tuple(leaf["id"])
        if identity in selected:
            add(
                identity,
                Blade(**leaf["blade"]).mesh(leaf_segments, 2),
                leaf["origin"],
                leaf["frame"],
                6,
            )
    if (1, 0) in selected:
        mesh = axis_mesh(
            recipe["stem_length_m"], recipe["stem_bend_rad"], 0.001, 0.0004, 12, 6
        )
        add((1, 0), mesh, np.zeros(3), UPRIGHT, 5)
    for i, stalk in enumerate(recipe["stalks"]):
        node, end = np.asarray(stalk["node"]), np.asarray(stalk["end"])
        if (2, i) in selected:
            delta = end - node
            mesh = axis_mesh(np.linalg.norm(delta), 0, 0.0003, 0.00015, 2, 5)
            add((2, i), mesh, node, normal_frame(delta, (0, 0, 1)), 5)
        if (3, i) in selected:
            scale = stalk["scale"]
            mesh = Bellflower(0.0075 * scale, 0.0035 * scale).mesh(
                bell_segments, bell_sides
            )
            add((3, i), mesh, end, np.eye(3), 7)
    return assembly.finish(), ranges, recipe
