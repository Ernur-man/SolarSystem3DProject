#!/usr/bin/env python3
"""Print official asset sources; deliberately does not fetch unverified image licenses."""
from pathlib import Path

ASSETS = Path(__file__).resolve().parents[1] / "assets"

def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    print("No image assets are bundled. Consult NASA Solar System Exploration:")
    print("https://solarsystem.nasa.gov/planets/overview/")
    print("NASA Image and Video Library: https://images.nasa.gov/")
    print("Check the individual item's usage notes before downloading or redistributing.")

if __name__ == "__main__":
    main()
