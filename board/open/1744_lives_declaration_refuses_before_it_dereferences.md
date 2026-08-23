Type: bug
Area: clients

**Live's declaration refuses before it dereferences, and its exposure constants carry origin**

Two small notes on src/clients/Live.cpp:

1. :98-100 — `Declared_.Surfacing.front()` runs whenever `Built != nullptr` and `Stands`
   is empty. The DEFAULT declaration ships one Material (Live.h:41), but a client that
   passes an empty vector gets `front()` on an empty vector: UB, not a refusal. Refuse at
   Build with the reason, or make emptiness unspellable in the Declaration type.

2. :165-167 — `std::log2(KeyLux / 2.5)` and `1.0 / (1.2 * pow(2.0, ev100))` carry no
   origin. Both are ISO-12232/Frostbite photometric calibration constants (2.5 lx·s the
   illuminance-EV100 constant, 1.2 the saturation-based sensor headroom); the house rule is
   every number with origin and population — name them where they stand or hoist them as
   named constexpr with the derivation.

Proof for (1): a unit case handing Built with empty Surfacing and reading the refusal text.
