"""Bounded exact original-word rows; selected gathers, no storage/cache policy."""

from dataclasses import dataclass

import numpy as np
import vertex_colour_palette as palette

MAX_ROWS = 262144
MAX_BYTES = 8 * 1024**2


class AttributeLimit(ValueError):
    pass


@dataclass(frozen=True)
class WordRows:
    count: int
    columns: int
    mode: str
    payload: bytes
    table: np.ndarray
    indices: object

    @property
    def nbytes(self):
        return len(self.payload)

    @property
    def shape(self):
        return self.count, self.columns

    @property
    def dtype(self):
        return np.dtype("<f4")

    def __getitem__(self, item):
        rows, columns = item if isinstance(item, tuple) else (item, slice(None))
        rows = np.asarray(rows)
        if (
            rows.dtype.kind not in "iu"
            or rows.size > MAX_ROWS
            or (rows.size and (rows.min() < 0 or rows.max() >= self.count))
        ):
            raise ValueError("Bounded existing original vertex rows required")
        if self.mode == "direct":
            values = self.table[rows]
        elif self.mode == "constant":
            values = np.empty((*rows.shape, self.columns), "<u4")
            values[...] = self.table[0]
        else:
            values = self.table[:, : self.columns][self.indices[rows]]
        return values.view("<f4")[..., columns]


def restore(payload, count, columns, mode):
    """Open one immutable snapshot; validate spans/indices before any gather."""
    if (
        type(count) is not int
        or not 0 <= count <= MAX_ROWS
        or type(columns) is not int
        or not 1 <= columns <= 4
        or len(payload) > MAX_BYTES
    ):
        raise ValueError("Bounded original row layout required")
    payload = bytes(payload)
    if mode == "direct":
        if len(payload) != count * columns * 4:
            raise ValueError("Direct original row span changed")
        table, indices = np.frombuffer(payload, "<u4").reshape(count, columns), None
    elif mode == "constant":
        if not count or len(payload) != columns * 4:
            raise ValueError("Constant original row span changed")
        table, indices = np.frombuffer(payload, "<u4").reshape(1, columns), None
    elif mode in ("palette16", "palette32"):
        table, indices = palette.view(payload)
        width = int(np.frombuffer(payload, "<u4", 4)[3])
        if (
            len(indices) != count
            or mode != "palette" + str(width)
            or np.any(table[:, columns:] != 0)
        ):
            raise ValueError("Palette original row layout changed")
    else:
        raise ValueError("Explicit direct/constant/palette representation required")
    return WordRows(count, columns, mode, payload, table, indices)


def prepare(values, mode="adaptive", *, bytes_most=MAX_BYTES):
    """Reuse the four-word codec; narrow rows are zero-padded without quantizing."""
    a = np.asarray(values)
    if (
        a.dtype != np.dtype("<f4")
        or a.ndim != 2
        or not 1 <= a.shape[1] <= 4
        or len(a) > MAX_ROWS
        or mode not in ("adaptive", "direct", "constant", "palette16", "palette32")
        or type(bytes_most) is not int
        or not 0 <= bytes_most <= MAX_BYTES
    ):
        raise ValueError("Bounded original Float32 rows and encoding choice required")
    a = np.ascontiguousarray(a)
    words = a.view("<u4")
    constant = bool(len(a) and np.all(words == words[:1]))
    if mode == "constant" and not constant:
        raise ValueError("Constant representation requires identical original words")
    chosen = mode
    if mode == "constant" or (mode == "adaptive" and constant):
        payload, chosen = words[0].tobytes(), "constant"
    elif mode == "direct" or not len(a):
        payload, chosen = words.tobytes(), "direct"
    else:
        padded = np.zeros((len(a), 4), np.uint32)
        padded[:, : a.shape[1]] = words
        width = None if mode == "adaptive" else int(mode[-2:])
        payload = palette.encode(padded, width)
        chosen = "palette" + str(int(np.frombuffer(payload, "<u4", 4)[3]))
        if mode == "adaptive" and len(payload) >= a.nbytes:
            payload, chosen = words.tobytes(), "direct"
    if len(payload) > bytes_most:
        raise AttributeLimit("attribute_byte_budget; retain original product")
    return restore(payload, len(a), a.shape[1], chosen)
