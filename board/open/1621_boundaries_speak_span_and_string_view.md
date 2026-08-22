Type: task
Area: src
Tags: hygiene, optimisation

# Boundaries speak span and string_view

Every read-only boundary that today takes `const std::vector<T>&` or `const std::string&`
takes `std::span<const T>` / `std::string_view` instead, and no call site copies into an
owning container just to traverse. Owner directive 2026-08-22; the architecture reviewer's
mechanical bar enforces it on every touched file from now on — this item is the sweep over
the existing tree.

Survey first (`grep -rn 'const std::vector<.*> &\|const std::string &' src/ include/`),
convert layer by layer, fast gate green per layer. `[[nodiscard]]`, `explicit`, `noexcept`
and `constexpr` hygiene ride the same sweep where the touched signature is missing them.
