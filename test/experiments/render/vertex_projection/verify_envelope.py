"""Check binary64 envelope endpoints against rational bounds and binary32 evaluations."""

import argparse
import json
import math
import random
import struct
from fractions import Fraction as Q

TINY = 2.0**-126


def bits(value):
    return struct.unpack("I", struct.pack("f", value))[0]


def adjacent(value, up):
    if value == 0:
        return 2.0**-149 if up else -(2.0**-149)
    word = bits(value)
    word += 1 if (value > 0) == up else -1
    return struct.unpack("f", struct.pack("I", word))[0]


def neighbours(value):
    guess = struct.unpack("f", struct.pack("f", float(value)))[0]
    return (
        adjacent(guess, False) if Q(guess) > value else guess,
        adjacent(guess, True) if Q(guess) < value else guess,
    )


class Arithmetic:
    def __init__(self, mode, flush):
        self.mode, self.flush = mode, flush
        self.operations = 0

    def input(self, value):
        return 0.0 if self.flush and abs(value) < TINY else value

    def rounded(self, value):
        self.operations += 1
        low, high = neighbours(value)
        if self.mode == "down":
            result = low
        elif self.mode == "up":
            result = high
        elif self.mode == "zero":
            result = low if value >= 0 else high
        else:
            dl, dh = value - Q(low), Q(high) - value
            result = low if dl < dh or (dl == dh and bits(low) % 2 == 0) else high
        return self.input(result)

    def add(self, a, b):
        return self.rounded(Q(self.input(a)) + Q(self.input(b)))

    def mul(self, a, b):
        return self.rounded(Q(self.input(a)) * Q(self.input(b)))

    def sum(self, values):
        while len(values) > 1:
            values = [
                self.add(values[i], values[i + 1]) for i in range(0, len(values) - 1, 2)
            ] + (values[-1:] if len(values) % 2 else [])
        return values[0]


def terms(case, row):
    model, view, point = case["model"], case["view"], case["point"][:3] + [1.0]
    result = [
        (view[4 * j + row], model[4 * k + j], point[k])
        for j in range(4)
        for k in range(4)
    ]
    return result + [
        (view[4 * j + row], (case["shift"][:3] + [0.0])[j]) for j in range(4)
    ]


def expanded(case, arithmetic, seed):
    randomizer = random.Random(seed)
    result = []
    for row in range(4):
        values = []
        for term in terms(case, row):
            factors = list(term)
            randomizer.shuffle(factors)
            value = factors.pop()
            for factor in factors:
                value = arithmetic.mul(value, factor)
            values.append(value)
        randomizer.shuffle(values)
        result.append(arithmetic.sum(values))
    return result


def precomposed(case, arithmetic):
    model, view = case["model"], case["view"]
    point, shift = case["point"][:3] + [1.0], case["shift"][:3] + [0.0]
    result = []
    for row in range(4):
        coefficients = [
            arithmetic.sum(
                [arithmetic.mul(view[4 * j + row], model[4 * k + j]) for j in range(4)]
            )
            for k in range(4)
        ]
        projected = arithmetic.sum(
            [arithmetic.mul(c, p) for c, p in zip(coefficients, point)]
        )
        shifted = arithmetic.sum(
            [arithmetic.mul(view[4 * j + row], shift[j]) for j in range(4)]
        )
        result.append(arithmetic.add(projected, shifted))
    return result


def inside(bounds, values):
    return all(
        Q(low) <= Q(value) <= Q(high)
        for (low, high), value in zip(bounds, values, strict=True)
    )


def family_limits(maximum):
    """The existing #990 expression family's exact relative/noise limits."""
    unit = Q(1, 2**23)
    gamma, growth = 21 * unit / (1 - 21 * unit), 1 / (1 - 59 * unit)
    noise = 238**3 * Q(1, 2**126) * maximum**2 * growth
    return gamma, growth, noise


def rational_bound(case):
    values = case["model"] + case["view"] + case["shift"][:3] + case["point"][:3]
    maximum = max(Q(1), *(abs(Q(v)) for v in values))
    gamma, _, noise = family_limits(maximum)
    for row, (low, high) in enumerate(case["clip"]):
        exact_terms = [
            math.prod(Q(value) for value in term) for term in terms(case, row)
        ]
        centre, mass = sum(exact_terms), sum(abs(value) for value in exact_terms)
        assert Q(low) <= centre - gamma * mass - noise
        assert Q(high) >= centre + gamma * mass + noise


def main():
    if not __debug__:
        raise RuntimeError("verification requires Python assertions enabled")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("result")
    args = parser.parse_args()
    with open(args.result, encoding="utf-8") as stream:
        data = json.load(stream)
    cases = {row["name"]: row for row in data["enclosures"] if row["unknown"] is None}
    evaluated, operations, gpu = 0, 0, 0
    for case in cases.values():
        rational_bound(case)
        for mode in ("nearest", "zero", "down", "up"):
            for flush in (False, True):
                arithmetic = Arithmetic(mode, flush)
                for values in (
                    expanded(case, arithmetic, 990),
                    expanded(case, arithmetic, 1015),
                    precomposed(case, arithmetic),
                ):
                    assert inside(case["clip"], values), (case["name"], mode, flush)
                    evaluated += 1
                operations += arithmetic.operations
    for row in data["rows"]:
        if row["name"] not in cases:
            continue
        case = cases[row["name"]]
        assert inside(case["clip"], row["gpu"])
        if "ndc" in case and row["gpu"][3] > 0:
            ratios = [Q(v) / Q(row["gpu"][3]) for v in row["gpu"][:3]]
            assert inside(case["ndc"], ratios)
        gpu += 1
    print(
        json.dumps(
            {
                "rational_bounds": len(cases),
                "evaluations": evaluated,
                "primitive_roundings": operations,
                "gpu_outputs": gpu,
                "false_bounds": 0,
                "page_suppression_qualified": False,
            }
        )
    )


if __name__ == "__main__":
    main()
