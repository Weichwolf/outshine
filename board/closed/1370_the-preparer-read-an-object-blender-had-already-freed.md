Type: bug
Area: corpus
Tags: oracle, instrument

**The preparer read an object Blender had already freed**

```
imported = [obj for obj in imported if obj.name in bpy.data.objects]
ReferenceError: StructRNA of type Object has been removed
```

**The filter that was meant to drop the removed objects was reading them.** `strip_crossings` deletes
from the scene; the line after it asks every original reference for its `name`, and a freed `StructRNA`
raises rather than returning anything. **The survivors are now NAMED BEFORE the removal** and looked up
by name afterwards, which touches no freed reference at all.

## Why it had never fired

**No subject the corpus could reach had anything stripped.** `strip_crossings` removes objects a case
declares out of the scene, and every case that reached the preparer had none to remove. **`Duck` is the
first — and `Duck` was unreachable until the licence stopped gating the corpus the same round.**

*Two defects, and the second was only findable once the first was gone. That is the ordinary shape of a
gate: it does not only block the thing it names, it hides everything behind it.*
