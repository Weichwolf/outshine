"""Admit complete replacement components before generating their children."""


def ordinals(ranges):
    previous = 0
    for first, count in ranges:
        if first < previous or count <= 0:
            raise ValueError("ordered nonempty source intervals required")
        yield from range(first, first + count)
        previous = first + count


def admit(parents, children):
    owners = {}
    roots = list(range(len(parents)))

    def root(at):
        while roots[at] != at:
            roots[at] = roots[roots[at]]
            at = roots[at]
        return at

    for at, parent in enumerate(parents):
        for source in ordinals(parent):
            if source in owners:
                raise ValueError("original parents overlap")
            owners[source] = at
    child_owners = []
    seen = set()
    for child in children:
        touched = set()
        for source in ordinals(child["source_ranges"]):
            if source not in owners or source in seen:
                raise ValueError("child coverage is foreign or overlapping")
            seen.add(source)
            touched.add(owners[source])
        if not touched:
            raise ValueError("empty child coverage")
        first = next(iter(touched))
        for at in touched:
            roots[root(at)] = root(first)
        child_owners.append(first)
    expected = {}
    for source, owner in owners.items():
        expected[root(owner)] = expected.get(root(owner), 0) + 1
    counts = {}
    blocked = set()
    for child, owner in zip(children, child_owners):
        component = root(owner)
        counts[component] = counts.get(component, 0) + sum(count for _, count in child["source_ranges"])
        if child["kind"] == "unknown" or not child.get("available", True):
            blocked.add(component)
    accepted = {at for at, count in expected.items()
                if counts.get(at, 0) == count and at not in blocked}
    return {
        "generate": [at for at, owner in enumerate(child_owners) if root(owner) in accepted],
        "keep_parents": [at for at in range(len(parents)) if root(at) not in accepted],
    }
