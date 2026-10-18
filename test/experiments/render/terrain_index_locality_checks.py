"""Check complete oriented skirt triangles and independent cache-model controls."""

from terrain_index_locality import batch_vertices, cache_misses, compare, index_order, oriented_triangles


def main():
    checks = 0
    for side in (2, 3, 5, 9, 17, 33):
        original = index_order(side, "interleaved")
        assert len(original) == 4 * (side - 1) * 6
        assert len(set(original)) == 8 * side - 4
        expected = oriented_triangles(original)
        for name in ("edge_runs", "perimeter"):
            candidate = index_order(side, name)
            assert oriented_triangles(candidate) == expected
            assert len(candidate) == len(original)
            assert max(candidate) < side * side + 4 * side
            checks += 1
        reversed_face = original[:]
        reversed_face[0], reversed_face[1] = reversed_face[1], reversed_face[0]
        assert oriented_triangles(reversed_face) != expected
        assert oriented_triangles(original[:-3]) != expected
        assert oriented_triangles(original + original[:3]) != expected
        checks += 4
    for lru in (False, True):
        assert cache_misses((0, 1, 2, 0, 1, 2), 2, lru) == 6
        assert cache_misses((0, 1, 2, 0, 1, 2), 3, lru) == 3
        checks += 2
    assert batch_vertices((0, 1, 2, 0, 1, 2), 1) == 6
    assert batch_vertices((0, 1, 2, 0, 1, 2), 2) == 3
    checks += 2
    result = compare(33)["variants"]
    assert result["perimeter"]["indices"] == result["meshoptimizer"]["indices"]
    checks += 1
    print(f"{checks}/{checks} terrain index controls PASS; no GPU prediction")


if __name__ == "__main__":
    main()
