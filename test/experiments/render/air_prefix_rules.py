"""Compare #252's thin moment on unchanged coefficient-prefix state transitions."""

import numpy as np
from air_path_prefix import AirCells, cell_integral, optical_depth
from compare_air_cell_rules import gauss, moment_rule, restrict


class CellRule:
    def __init__(self, mode):
        if mode not in ("four", "moment", "exact32", "exact64"):
            raise ValueError("declared cell comparison rule required")
        self.mode = mode
        self.work = {
            "calls": 0,
            "zero_calls": 0,
            "thin_channels": 0,
            "four_channels": 0,
            "nodes": 0,
        }

    def __call__(self, cells, index, distance):
        self.work["calls"] += 1
        if distance == 0:
            self.work["zero_calls"] += 1
            return np.zeros(3), np.ones(3)
        if self.mode == "four":
            self.work["four_channels"] += 3
            self.work["nodes"] += 12
            return cell_integral(cells, index, distance)
        width = cells.edges[index + 1] - cells.edges[index]
        tau = optical_depth(cells.extinction[index], distance / width, width)
        thin = tau <= 0.001
        self.work["thin_channels"] += int(thin.sum())
        self.work["four_channels"] += int((~thin).sum())
        self.work["nodes"] += int((~thin).sum()) * 4
        if not thin.any():
            return cell_integral(cells, index, distance)
        rows = np.column_stack(
            (
                np.full(3, width),
                np.full(3, distance),
                cells.extinction[index].T,
                cells.emission[index].T,
            )
        )
        sigma, source = restrict(rows[thin])
        lengths = np.full(int(thin.sum()), distance)
        light = np.empty(3)
        if self.mode == "moment":
            light[thin] = moment_rule(lengths, sigma, source)
        else:
            order = int(self.mode.removeprefix("exact"))
            light[thin] = gauss(lengths, sigma, source, order)
            self.work["nodes"] += int(thin.sum()) * order
        if (~thin).any():
            coarse = AirCells(
                np.array([0.0, width]),
                cells.extinction[index : index + 1, :, ~thin],
                cells.emission[index : index + 1, :, ~thin],
                0,
            )
            light[~thin], _ = cell_integral(coarse, 0, distance)
        return light, np.exp(-tau)
