Scripts that measured Link's head meshes and baked the hood shell table (sHoodShell) in
LinkGanonCape.cpp. Requires Python 3 with numpy and Pillow.

  set OOT_O2R=C:\Spel\soh\oot.o2r
  set STRHASH64_CPP=<Shipwright checkout>\libultraship\src\ship\utils\StrHash64.cpp
  python hoodbake.py     -> renders hood_close.png (face close-ups)

hoodbake.py: bake() builds the shell, sheet()/closeups() render previews, pierce_test() checks that
no head vertex sticks out. The C table was printed from bake() for both ages (see LinkGanonCape.cpp).
