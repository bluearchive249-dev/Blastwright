# gdf_castle.o2r generator ("Play as Ganondorf", Hyrule Castle interior)

Builds the castle interior scene (SCENE_TESTROOM slot) and the working drawbridge as a mods-folder archive.
It writes only our own geometry; textures and the courtyard outside the throne room window are referenced by
path into the player's own `oot.o2r`, which these scripts read at build time (set its path in `otr.py`).

    python3 castle.py out/gdf_castle.o2r

Put `gdf_castle.o2r` in the game's `mods` folder.
