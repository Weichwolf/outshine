"""Current-policy steady-repeat material states on preserved native UV/colour fields."""

import numpy as np
from building_mass_materials import material


def face_materials(source, at):
    positions = source["positions"].astype(float)[source["indices"]]
    uv, paint = (source[name][source["indices"]] for name in ("uv", "colour"))
    e, f = positions[:, 1] - positions[:, 0], positions[:, 2] - positions[:, 0]
    normal = np.cross(e, f)
    length = np.linalg.norm(normal, axis=1)
    square = np.einsum("ij,ij->i", normal, normal)
    delta = uv[:, 1:, 1].astype(float) - uv[:, :1, 1]
    numerator = delta[:, 0, None] * np.cross(f, normal) + delta[:, 1, None] * np.cross(
        normal, e
    )
    gradient = np.divide(
        numerator,
        square[:, None],
        out=np.zeros_like(numerator),
        where=square[:, None] > 0,
    )
    normal /= np.where(length > 0, length, 1)[:, None]
    colours = np.zeros((len(positions), 2, 3))
    weights = np.zeros((len(positions), 2, len(at["palette"])))
    supported = np.zeros(len(positions), bool)
    needs = []
    for i in range(len(positions)):
        try:
            if length[i] == 0:
                raise ValueError("degenerate original triangle")
            u = uv[i, :, 0]
            if u[0] < 0:
                if not np.all(u == u[0]):
                    raise ValueError("nonconstant negative material category")
                code = int(u[0])
            else:
                code = int(np.trunc(u[0] / 256))
                if np.any(np.trunc(u / 256) != code):
                    raise ValueError("original triangle crosses a material category")
            if not np.all(paint[i] == paint[i, 0]):
                raise ValueError("nonconstant original paint factor")
            for above in (0, 1):
                colours[i, above], weights[i, above] = material(
                    at,
                    code,
                    int(source["binding"][i, 4]),
                    paint[i, 0, :3],
                    openings=bool(above and np.ptp(u) > 0),
                    storeys_per_m=float(np.linalg.norm(gradient[i])) if above else 0,
                    slope=float(np.linalg.norm(gradient[i])),
                )
            supported[i] = True
        except ValueError as error:
            needs.append({"face": i, "reason": str(error)})
    return {
        "albedo": colours,
        "weights": weights,
        "supported": supported,
        "normal": normal,
        "uv_gradient": gradient,
    }, needs
