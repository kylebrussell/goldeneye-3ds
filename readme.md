# GoldenEye 007 — native Nintendo 3DS port

This repository is an in-progress native Nintendo 3DS port built on the
[`n64decomp/007`](https://github.com/n64decomp/007) GoldenEye decompilation.
The port keeps original decompiled gameplay systems and data paths wherever
possible, with platform adapters for 3DS rendering, input, audio, storage, and
timing.

The port is not yet a finished or fully playable release. Dam currently runs
natively in Azahar and on 3DS-targeted builds with original movement,
collision, guards, weapons, mission state, HUD, menus, and audio under active
integration. See [the Nintendo 3DS port notes](./docs/Porting3DS.md) for the
current verified state and remaining gaps.

No game ROM, extracted proprietary assets, generated asset pack, or compiled
game binary is included. You must provide your own legally obtained NTSC-U ROM
to generate the required local assets.

[![NTSC-Status][NTCS-badge]][NTCS-link]
[![JP-Status][JP-badge]][JP-link]
[![PAL-Status][PAL-badge]][PAL-link]

[NTCS-link]: https://kholdfuzion.github.io/goldeneyestatus/
[NTCS-badge]: ../../workflows/NTSC-Status/badge.svg

[JP-link]: https://kholdfuzion.github.io/goldeneyestatus/JPN.htm
[JP-badge]: ../../workflows/JP-Status/badge.svg

[PAL-link]: https://kholdfuzion.github.io/goldeneyestatus/EU.htm
[PAL-badge]: ../../workflows/EU-Status/badge.svg

## About this repository

This is a WIP decompilation of Goldeneye 007!

It builds the following ROMs:

* ge007.u.z64 `sha1: abe01e4aeb033b6c0836819f549c791b26cfde83`
* ge007.j.z64 `sha1: 2a5dade32f7fad6c73c659d2026994632c1b3174`
* ge007.e.z64 `sha1: 167c3c433dec1f1eb921736f7d53fac8cb45ee31`

**Note: This repository does not include all assets necessary for compiling the ROMs. A prior copy of the game is required to extract the assets.**

## Documentation

* [Setup Guide:](./docs/SetupGuide.md) useful information about installing the necessary dependencies and how to use it
* [Structure Guide:](./docs/StructureGuide.md) learn more about the project structure of this repository
* [Style Guide:](./docs/StyleGuide.md) code style conventions if you want to contribute code
* [Nintendo 3DS native port:](./docs/Porting3DS.md) build, asset staging, emulator/hardware testing, current milestones, and known blockers. The `.3dsx` is an active bring-up build, not a fully playable game yet.
