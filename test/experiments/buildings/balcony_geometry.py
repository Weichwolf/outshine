"""Same shared door cell plus one reused closed deck/guard and a provisional arrival pocket."""

import numpy as np
from building_mesh import solid
from facade_entry_geometry import basis_for, doorway, expand


def products(current, owner, material, mode):
    frame = current["doorway"]["frame"]
    basis = basis_for(frame)
    local = np.array([basis[0], -basis[2], basis[1]])
    centre = current["doorway"]["door"]["centre"]
    origin = np.r_[centre, current["floor_z_m"]]
    fixed = owner.template(current["prototype"])
    raw = {
        "vertices": fixed["vertices"] @ local + origin,
        "faces": fixed["faces"],
        "materials": np.array([4, 7], dtype=np.uint8)[fixed["roles"]],
    }
    output = [expand(raw, material)]
    output.extend(doorway(current["doorway"], material, mode, base_m=current["base_m"]))
    if mode == "through":
        for bottom, top, component in (
            (current["base_m"], current["floor_z_m"], 0),
            (
                current["base_m"] + current["doorway"]["floor_m"] - 0.18,
                current["base_m"] + current["doorway"]["floor_m"],
                2,
            ),
        ):
            storage = [], [], [], []
            solid(storage, current["pocket"], bottom, top, component)
            output.append(
                expand(
                    {
                        "vertices": np.asarray(storage[0]),
                        "faces": np.asarray(storage[1]),
                        "materials": np.asarray(storage[2]),
                    },
                    material,
                )
            )
    return output
