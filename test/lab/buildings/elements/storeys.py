"""THE STOREY RHYTHM: a facade is not one storey repeated, and that is most of what an epoch is.

Five identical floors is the loudest thing left on a generated block. Every period before the
1920s builds a HIERARCHY, and a viewer reads the period from it before they read any ornament:

    SOCKELGESCHOSS  the ground floor: a shop or a service floor, its own height, small windows or
                    a wide shop light, and a plinth under it
    BELETAGE        the first floor above it, the piano nobile: TALLER than the rest, its windows
                    taller in proportion, and the only floor that gets a pediment over them. It
                    is where the owner lived, and every Gruenderzeit block on the planet says so
    REGELGESCHOSSE  the floors above: each a little lower than the one below, and their windows
                    a little shorter -- the diminution is small (3 to 5 percent a floor) and it is
                    what makes a facade look built rather than stacked
    DACHGESCHOSS    the attic under the eaves: lowest of all, small square windows

The numbers are proportions and not heights, so the same rhythm works on a 3.6 m Gruenderzeit
storey and a 2.5 m Siedlungshaus one. A period that has no hierarchy -- postwar, late20,
contemporary, industrial -- gets a flat rhythm, and that flatness is ITS period's own statement.
"""

# per epoch: (beletage height as a share of the standard, its window's share, the diminution per
# floor above it, whether the beletage gets a pediment, whether the attic is squat)
RHYTHM = {
    "gruenderzeit":   (1.22, 1.20, 0.045, True, True),
    "jugendstil":     (1.18, 1.18, 0.040, True, True),
    "baroque":        (1.30, 1.28, 0.055, True, True),
    "gothic":         (1.00, 1.00, 0.000, False, False),
    "sacral":         (1.00, 1.00, 0.000, False, False),
    "interwar":       (1.10, 1.08, 0.030, False, True),
    "commercial":     (1.14, 1.16, 0.030, False, False),
    "postwar":        (1.00, 1.00, 0.010, False, False),
    "late20":         (1.00, 1.00, 0.000, False, False),
    "contemporary":   (1.00, 1.00, 0.000, False, False),
    "industrial":     (1.00, 1.00, 0.000, False, False),
    "hall":           (1.00, 1.00, 0.000, False, False),
    "tower":          (1.00, 1.00, 0.000, False, False),
    "farm":           (1.00, 1.00, 0.000, False, False),
    "siedlungshaus":  (1.04, 1.05, 0.020, False, True),
    "bungalow":       (1.00, 1.00, 0.000, False, False),
    "einfamilienhaus": (1.02, 1.04, 0.015, False, True),
}


class Storey:
    """One floor of a facade: where it starts, how tall it is, and the opening it carries."""

    __slots__ = ("at", "z0", "height", "win_w", "win_h", "sill", "head", "kind")

    def __init__(self, at, z0, height, win_w, win_h, sill, head, kind):
        self.at, self.z0, self.height = at, z0, height
        self.win_w, self.win_h, self.sill = win_w, win_h, sill
        self.head, self.kind = head, kind

    def __repr__(self):
        return (f"Storey({self.kind} at {self.z0:.2f}, h {self.height:.2f}, "
                f"win {self.win_w:.2f}x{self.win_h:.2f}, head {self.head})")


def of(ctx):
    """THE FACADE'S STOREYS, top to bottom, fitted to the wall's own height.

    The rhythm gives PROPORTIONS; the wall gives the height. The shares are normalised so the
    storeys fill the wall exactly -- a facade whose floors do not add up to its own eaves is the
    defect this replaces."""
    levels = max(1, int(ctx.levels))
    tall, wide, fade, pediment, squat = RHYTHM.get(ctx.epoch, RHYTHM["late20"])
    shares, kinds = [], []
    for level in range(levels):
        if level == 0:
            shares.append(1.06 if levels > 1 else 1.0)
            kinds.append("sockel")
        elif level == 1 and levels >= 3:
            shares.append(tall)
            kinds.append("beletage")
        elif squat and level == levels - 1 and levels >= 4:
            shares.append(0.86)
            kinds.append("dach")
        else:
            shares.append(max(0.72, 1.0 - fade * (level - 1)))
            kinds.append("regel")
    total = sum(shares)
    unit = ctx.place.height / total if total > 0 else ctx.level_m
    out, z = [], 0.0
    for level in range(levels):
        h = unit * shares[level]
        kind = kinds[level]
        # THE WINDOW IS THE SIZE OF ITS ROOM: its height is a share of the storey it stands in,
        # and its sill is where a person's hand rests, which is why a sill is 0.85 to 0.95 m
        # whatever the floor is
        grow = wide if kind == "beletage" else (0.86 if kind == "dach" else 1.0)
        win_h = min(ctx.win_h * grow, h - ctx.sill_m - 0.28)
        win_w = ctx.win_w * (1.0 + 0.06 * (grow - 1.0))
        head = "pediment" if (kind == "beletage" and pediment) else (
            "straight" if ctx.cornice else "none")
        out.append(Storey(level, z, h, win_w, max(0.5, win_h), ctx.sill_m, head, kind))
        z += h
    return out
