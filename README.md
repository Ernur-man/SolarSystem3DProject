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
- `G`: toggle bloom rendering (the HDR and tone-mapping path remains active); `L`: toggle screen-space gravitational lensing; `I`: toggle the Instancing HUD state.
- `Q`: switch High / Low quality. High uses full-resolution HDR, eight blur iterations, and 32 x 48 sphere segments; Low uses 0.75-resolution HDR, four blur iterations, and 8 x 12 sphere segments. `B` summons the black hole or reverses it back out of the system.
- `R`: return the hole in Free Explore, or restart an active/finished mission (reset then automatic approach); `F`: toggle free-fly; `[` / `]`: rotate the Sun light direction.
- `Esc`: clear selection and smoothly return to the overview; `F12`: save a PNG in `screenshots/`.
- `F1` or `H`: show or hide the complete controls overlay.
- Left click or select a name in Mission Control: select a planet or the Sun and fly to it. Right mouse drag rotates the camera; the wheel zooms.
- In free-fly, `W` / `A` / `S` / `D` move and `Shift` moves faster.
- The start screen offers **Start Mission** and **Free Explore**. Mission visits and correct quiz answers earn points while the threat advances; quiz questions pause simulation time.

The window can be resized and safely handles a minimized framebuffer. Team name is a placeholder.

## Data and image provenance

`assets/planets.json` and `assets/quiz.json` contain educational facts and quiz questions. Planetary values are approximate and checked against NASA's [Planetary Fact Sheet - Metric](https://nssdc.gsfc.nasa.gov/planetary/factsheet/) and [Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html?level=1). The moon counts reflect that fact sheet's March 2025 update. No texture files are included. To use the optional sky image, place `stars_milky_way.jpg` at `assets/textures/stars_milky_way.jpg`; without it, the procedural starfield is used. Solar System Scope textures are licensed CC BY 4.0 and require attribution: [Solar System Scope Textures](https://www.solarsystemscope.com/textures/). The texture is not downloaded automatically and must not be committed.

## Feature status

**Done:** The gameplay/camera module, HDR and bloom pipeline, and black-hole module: orbit/free-fly cameras, picking and fly-to, planet info and quiz, mission score/win/lose/restart, HDR bright-pass and Gaussian bloom, ACES tone mapping, and sRGB output. The black-hole module includes the Idle/Approaching/Consuming/Returning threat state machine, eased body pull/stretch/swallow effects, the event horizon and procedural accretion disk, and a screen-space gravitational lensing pass. Mission mode summons the hole automatically; Free Explore starts with it Idle. `B` summons or returns the hole, `R` restarts the mission, and `L` toggles lensing. `G` toggles only the bloom contribution; `Q` changes HDR scale, blur iterations, and sphere detail.

**Partial:** `I` changes HUD state only; instancing is not implemented. The old timer threat remains as an explicitly marked fallback alternative; the real black hole is selected by default. Scene sizes and distances are stylized, and the lensing/spaghettification are illustrative screen-space and transform effects rather than a physical simulation.

**Missing:** No requested black-hole module items are missing. Decorative planetary rings and instancing are outside this module.

Build uses CMake FetchContent and requires dependency sources at configure time. A successful clean-source verification is reported with the project handoff, not assumed from this README.
