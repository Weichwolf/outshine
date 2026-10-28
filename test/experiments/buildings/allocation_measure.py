"""Scoped allocation evidence without importing a domain's generators or inputs."""


def allocations(path, call):
    import memray

    with memray.Tracker(path, trace_python_allocators=True, native_traces=True):
        value = call()
    reader = memray.FileReader(path)
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    count, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in frees and event.size > 0:
            count += event.n_allocations
            requested += event.size
    return value, {
        "allocation_events": count,
        "requested_bytes": requested,
        "scoped_high_watermark_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "profiler": f"memray {memray.__version__}",
        "trace": path.name,
    }
