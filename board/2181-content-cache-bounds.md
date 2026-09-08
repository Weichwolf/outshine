Type: fix
State: active
Area: world
Tags: cache, correctness

# Cache reads stay bounded and concurrent publications stay isolated

Found while connecting crown artifacts (2111): ContentStore::Read allocates the
entire file without a caller budget. Keep uses an instance-local counter with
fopen(wb), so two stores/processes can truncate the same temporary file.

Reuse the existing atomic rename path. Add an explicit optional read-byte limit,
reject oversized data before resize, and open temporary files exclusively with
bounded collision retries. Return publication success; existing world fetching
may still treat cache-write failure as nonfatal. C11 exclusive fopen mode preserves
an existing file; no overwrite to resolve a collision.

Unreal/RAGE derived-data principle: complete publication and bounded IO. Their
private file APIs are not asserted. Proof: small limit rejects a real crown file;
pre-existing temp contents remain unchanged while publication succeeds via another
name; two independent stores and failed destination publication do not expose a
partial artifact. Crown cache must reuse this path rather than duplicate it.
