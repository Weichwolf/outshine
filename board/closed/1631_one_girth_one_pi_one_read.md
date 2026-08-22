Type: task
Parent: 1611
Area: core

# The last circumference dies, pi stands once, and the engine reads a file in one place

Bundle task on the reviewer's round (issues 1628, 1630 and the 1611 sharpening):
World.cpp's kEarthCirc yields to the one Mercator girth; the three pi spellings collapse to
std::numbers::pi (C++23's own); Engine::Read/Load share one file-reading helper. Each closes
with the fast gate green and the grep that finds no second spelling.


---

Closed: World.cpp reads Data::kMercatorGirthM (the false claim of 0b4acae is repaid -- grep
finds no 40075016/40075017 in src or tools); pi is std::numbers::pi at all three former
spellings and kRad2Deg/kMsToKt derive instead of restating; Engine::ReadInto is the one
file-reading path under Read and Load. Fast gate 122/122 warm.