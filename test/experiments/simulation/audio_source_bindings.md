# Audio source binding

Owner: [#1322](https://github.com/Weichwolf/outshine/issues/1322).

`audio_source_bindings.py --output <system-temp>/comparison.json` compares repeated
scans, binary lookup in sorted IDs and prepared hash lookup. Twelve cases cover
16/128/1024 sources, ordered/shuffled/sparse inputs, duplicates and unknown IDs.
All return the same final active row; inactive duplicates do not replace it.

The hash variant wins this experiment. Native `Audio::SourceBindings` prepares
the identity index, routed gains and fixed row storage during configuration.
Each Mix resolves inputs once, outside its internal sample-block loop. Clearing
N rows and at most H hash queries replaces N×H comparisons per block and repeated
routing-name searches. Standard hash lookup has expected constant cost, not a
guaranteed worst-case bound. Configuration still uses the existing BusGraph.

Native counterexamples live in `test/outshine/src/audio/{SourceBindings,Mixer}/`:
pointer identity, gain composition, reconfiguration, duplicate/absent identities,
independent sample oracle and existing allocation/partition checks. The native
cost case compares the old scan and new binding on identical reversed inputs.
It reports binding times and work, not total mixing or callback admission.

Prepared identities do not solve voice selection, source virtualization, callback
ownership or room acoustics. Those remain [#2](https://github.com/Weichwolf/outshine/issues/2)
and the joint legacy review [#1320](https://github.com/Weichwolf/outshine/issues/1320).
The existing bounded-audio design is in
[doc/references/audio/bounded-spatial-audio.md](../../../doc/references/audio/bounded-spatial-audio.md).
