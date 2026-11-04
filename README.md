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
- `G`: bloom, `L`: lensing, `[` / `]`: orbit view rotation
- Click a planet name in Mission Control for its basic information; `Back` or `Esc` clears selection
- `F1`: controls overlay

The window can be resized and safely handles a minimized framebuffer. Team name is a placeholder.

## Data and image provenance

`assets/planets.json` and `assets/quiz.json` contain hand-authored educational facts and quiz questions. Planet measurements are approximate reference values; verify against NASA before formal submission. No texture files are included. To use the optional sky image, place `stars_milky_way.jpg` at `assets/textures/stars_milky_way.jpg`; without it, the procedural starfield is used. Solar System Scope textures are licensed CC BY 4.0 and require attribution: [Solar System Scope Textures](https://www.solarsystemscope.com/textures/). The texture is not downloaded automatically and must not be committed. NASA images may have item-specific usage notes; inspect each asset record before use.

## Feature status

**Done:** OpenGL 3.3 window, procedural shaded Sun and eight colored planets, animated stylized orbits, resizing/minimize handling, speed and pause controls, basic ImGui planet list, console shader diagnostics, separate GLSL files, procedural starfield sky background with an optional equirectangular sRGB texture.

**Partial:** Quality toggle is absent; planet info is abbreviated; quiz window does not yet run rounds; orbit lines, selection by ray-picking, smooth follow camera, planet textures, Sun corona, HDR framebuffer/bloom passes, lensing framebuffer composition, black hole event, mission scoring, screenshot output and game restart are not implemented. Bloom/lensing keys currently change HUD state only. The post shader demonstrates equations but is not connected to a post-process pass. Instancing, rings, Earth clouds/night lights/atmosphere, movable light, FPS call counts, GL debug callback and screenshot support are missing. These gaps mean this is a compilable starting demo, not a complete implementation of the assignment.

Build depends on fetching upstream repositories and has not been verified in this environment.
