# Street network pages

Measure how much solved street topology a moving scene can retain across a source
window change. The experiment reads two existing `street-network` assets from a
read-only store. It partitions directed connections by their origin node's tile.
Connections name their destination by coordinates so renumbering in the source
snapshot has no effect. The oracle reconstructs each graph, applies the page
replacement, and compares the complete directed edge multiset and node properties.

Run with Python 3.9 or newer and the `zstd` CLI for compressed packages. Pass two
unique network keys or prefixes from the local cache.

```sh
python3 -B lib/experiments/transport/network-pages/network_pages.py \
  build/assets/assets.sqlite 8ae1868529b2 07223534d944
```

On that Tokio flight transition, zoom 16 retains 663 of 882 next pages and
489,493 of 612,639 directed connections. Cross-page references account for
52,784 connections. The fixture with an absent destination checks that a valid
connection can wait for its destination page.

The comparison works on already solved topology. A generator that can produce
and certify individual pages independently still needs fixed ownership and a
boundary join rule. Global ordered snapping can propagate a change beyond a fixed
halo. See [#1956](https://github.com/Weichwolf/outshine/issues/1956).
