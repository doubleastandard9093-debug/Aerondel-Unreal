"""Runs every Gielinor setup step in order."""
import importlib
import create_input_assets
import generate_starter_data
import build_starter_town


def run():
    for m in (create_input_assets, generate_starter_data, build_starter_town):
        importlib.reload(m)
        m.run()
