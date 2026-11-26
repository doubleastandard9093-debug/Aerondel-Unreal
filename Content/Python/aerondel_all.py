"""Runs every Aerondel setup step in order."""
import importlib
import create_input_assets
import generate_starter_data
import build_smooth_shapes
import build_starter_town
import build_high_fidelity


def run(include_high_fidelity=True):
    steps = [create_input_assets, generate_starter_data, build_smooth_shapes, build_starter_town]
    if include_high_fidelity:
        steps.append(build_high_fidelity)
    for m in steps:
        importlib.reload(m)
        m.run()
