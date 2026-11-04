# Event Horizon: Solar System Explorer

A C++17 / OpenGL 3.3 Core university graphics project. The scene includes a procedural starfield background and runs without image assets. An optional equirectangular sky texture can be placed at `assets/textures/stars_milky_way.jpg`.

## Build and run (Windows 10/11, VS2022)

Install Visual Studio Community 2022 with **Desktop development with C++**, CMake, Git, and a working internet connection (CMake FetchContent downloads dependencies). In Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\solar_system.exe
```

The same configure/build commands work with GCC or Clang. In Visual Studio use **File → Open → Folder** and select this directory, or open the generated solution in `build`.

## Libraries

GLFW (Zlib/libpng), GLAD (MIT), GLM (Happy Bunny and MIT), Dear ImGui (MIT), stb (public domain / MIT). Dependencies are fetched at configure time. stb_image loads the optional sky texture.

## Controls

- `Space`: pause or resume simulation; `1` / `2` / `3`: simulation speed 1x / 5x / 10x.
- `G`: toggle Bloom HUD state; `L`: toggle Lensing HUD state; `I`: toggle Instancing HUD state.
- `Q`: toggle high / low sphere detail; `B`: toggle the black-hole HUD state. These are interface placeholders until the corresponding renderer and black-hole modules are connected.
- `R`: restart an active or finished mission; `F`: toggle free-fly; `[` / `]`: rotate the Sun light direction.
- `Esc`: clear selection and smoothly return to the overview; `F12`: save a PNG in `screenshots/`.
- `F1` or `H`: show or hide the complete controls overlay.
- Left click or select a name in Mission Control: select a planet or the Sun and fly to it. Right mouse drag rotates the camera; the wheel zooms.
- In free-fly, `W` / `A` / `S` / `D` move and `Shift` moves faster.
- The start screen offers **Start Mission** and **Free Explore**. Mission visits and correct quiz answers earn points while the threat advances; quiz questions pause simulation time.

The window can be resized and safely handles a minimized framebuffer. Team name is a placeholder.

## Data and image provenance

`assets/planets.json` and `assets/quiz.json` contain educational facts and quiz questions. Planetary values are approximate and checked against NASA's [Planetary Fact Sheet - Metric](https://nssdc.gsfc.nasa.gov/planetary/factsheet/) and [Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html?level=1). The moon counts reflect that fact sheet's March 2025 update. No texture files are included. To use the optional sky image, place `stars_milky_way.jpg` at `assets/textures/stars_milky_way.jpg`; without it, the procedural starfield is used. Solar System Scope textures are licensed CC BY 4.0 and require attribution: [Solar System Scope Textures](https://www.solarsystemscope.com/textures/). The texture is not downloaded automatically and must not be committed.

## Feature status

**Done:** Steps 1–7 and 9–12: orbit line loops; orbit and free-fly cameras; ray-sphere selection; eased fly-to/follow; planet information and quiz; a start screen with Free Explore and Mission; a fallback threat, scoring, terminal states, and restart; movable Sun light; help overlay; PNG screenshots; and OpenGL debug output in debug builds.

**Partial:** Step 8's HUD shows Bloom, Lensing, and Instancing toggle state and logs changes, but those switches are placeholders until the renderer modules are connected. `B` similarly changes the black-hole HUD placeholder only; it does not disable the fallback mission threat. Sphere detail toggling is implemented, while high-detail renderer features (HDR/bloom processing, lensing, instancing, rings, and detailed planet surfaces) remain outside this module. Scene sizes and distances are stylized.

**Missing:** None of steps 7–12 are missing from this gameplay/UI module. The black-hole visual and state machine are a teammate-owned integration and are not part of the fallback threat implementation here.

Build uses CMake FetchContent and requires dependency sources at configure time. A successful clean-source verification is reported with the project handoff, not assumed from this README.
