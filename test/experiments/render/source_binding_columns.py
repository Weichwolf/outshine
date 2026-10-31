"""Lossless selected reads of six recorded original-triangle binding columns."""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Columns:
    count: int
    columns: tuple
    modes: tuple

    @property
    def payload_bytes(self):
        return sum(column.nbytes for column in self.columns)

    def gather(self, indices):
        indices = np.asarray(indices)
        if (
            indices.ndim != 1
            or indices.dtype.kind not in "iu"
            or (len(indices) and (indices.min() < 0 or indices.max() >= self.count))
        ):
            raise ValueError("Bounded original triangle ordinals required")
        result = np.empty((len(indices), 6), dtype="<i4")
        for column, (values, mode) in enumerate(
            zip(self.columns, self.modes, strict=True)
        ):
            if mode == "constant":
                result[:, column] = values[0]
            elif mode == "bit":
                result[:, column] = (values[indices // 8] >> (indices % 8)) & 1
            else:
                result[:, column] = values[indices]
        return result


def encode(binding, *, bitpack=False):
    if (
        not isinstance(binding, np.ndarray)
        or binding.dtype != np.dtype("<i4")
        or binding.ndim != 2
        or binding.shape[1] != 6
        or not 1 <= len(binding) <= 1048576
    ):
        raise ValueError("Bounded complete native six-column Int32 binding required")
    columns, modes = [], []
    for col in binding.T:
        low, high = int(col.min()), int(col.max())
        if low == high:
            values, mode = np.array([low], dtype="<i4"), "constant"
        elif bitpack and low >= 0 and high <= 1:
            values, mode = np.packbits(col.astype("u1"), bitorder="little"), "bit"
        else:
            dtype = (
                "u1"
                if low >= 0 and high <= 255
                else "<u2"
                if low >= 0 and high <= 65535
                else "<i2"
                if low >= -32768 and high <= 32767
                else "<i4"
            )
            values, mode = col.astype(dtype), "direct"
        values.flags.writeable = False
        columns.append(values)
        modes.append(mode)
    return Columns(len(binding), tuple(columns), tuple(modes))
