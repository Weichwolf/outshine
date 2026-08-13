Type: feature
Area: corpus
Tags: oracle, khronos

**`grown.SHAPES` gains a name per generator kind**

`test/corpus/prep/grown.py` already builds the library and runs `test/corpus/GrowPart.cpp` over it, the
same door the preparer uses for Blender — so **a part the engine grew can already be a corpus case.**
What it holds is one name: `SHAPES = ("grown-tree",)`.

**So every species task is servable today and no other generated type is.** A roof form, a facade, a
crop row, a building mass have no shape name, and until they do their tasks cannot produce the render
case their acceptance requires.

**Done when** `SHAPES` carries a name per generator kind and one non-tree type produces a scoring case.
