"""Aerondel > BUILD FULL GAME WORLD: every setup step, then the whole World map (not the small test, not just the
starter town). Writes Saved/AerondelWorldBuild.json with mode "full" that Tools/package_win64.bat checks."""
import importlib
import aerondel_all
import aerondel_safe
import build_starter_town
import build_aerondel_regions
import world_placements
import build_world_blockout


def run():
    for m in (aerondel_all, aerondel_safe, build_starter_town, build_aerondel_regions, world_placements, build_world_blockout):
        importlib.reload(m)
    aerondel_all.run()                      # input assets, starter data, smooth shapes, StarterTown, high fidelity
    build_world_blockout.run(small=False)   # the whole web world + placements -> /Game/Gielinor/Maps/World
