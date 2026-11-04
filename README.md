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

- `1`, `2`, `3`: simulation speed 1x / 5x / 10x; `Space`: pause
- `G`: bloom, `L`: lensing
- Right mouse drag orbits the camera; the wheel zooms. `F` toggles free-fly, `WASD` moves, and `Shift` moves faster.
- Select a world in Mission Control or click it in the scene to open its information panel and fly to it. `Back` or `Esc` returns to the overview.
- Start Quiz opens five randomized questions for the selected world; simulation time pauses until the quiz is closed.
- `F1`: controls overlay

The window can be resized and safely handles a minimized framebuffer. Team name is a placeholder.

## Data and image provenance

`assets/planets.json` and `assets/quiz.json` contain educational facts and quiz questions. Planetary values are approximate and checked against NASA's [Planetary Fact Sheet - Metric](https://nssdc.gsfc.nasa.gov/planetary/factsheet/) and [Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html?level=1). The moon counts reflect that fact sheet's March 2025 update. No texture files are included. To use the optional sky image, place `stars_milky_way.jpg` at `assets/textures/stars_milky_way.jpg`; without it, the procedural starfield is used. Solar System Scope textures are licensed CC BY 4.0 and require attribution: [Solar System Scope Textures](https://www.solarsystemscope.com/textures/). The texture is not downloaded automatically and must not be committed.

## Feature status

**Done:** Orbit line loops; orbit and free-fly cameras; click ray-sphere selection with a highlight ring; eased fly-to and follow; Sun and all planet information from JSON with missing-field warnings; five-question randomized quizzes with feedback and score; simulation pauses during quizzes.

**Partial:** The user interface and data are functional but intentionally simple; planet sizes and orbital distances in the rendered scene are stylized. Quality controls, planet surface textures, Sun corona, HDR framebuffer/bloom passes, lensing composition, black hole event/state machine, mission scoring, screenshot output, restart, instancing, rings, Earth clouds/night lights/atmosphere, movable light, FPS call counts, and GL debug callback remain unimplemented. Bloom/lensing keys currently change HUD state only. The post shader demonstrates equations but is not connected to a post-process pass.

Build depends on fetching upstream repositories and has not been verified in this environment.
