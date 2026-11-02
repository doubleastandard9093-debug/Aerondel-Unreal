"""Runs every Aerondel setup step in order."""
import importlib
import create_input_assets
import generate_starter_data
import build_smooth_shapes
import build_starter_town
import build_high_fidelity


def run():
    for m in (create_input_assets, generate_starter_data, build_smooth_shapes, build_starter_town, build_high_fidelity):
        importlib.reload(m)
        m.run()
