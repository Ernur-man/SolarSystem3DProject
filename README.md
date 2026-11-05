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

**Done:** HDR rendering uses an RGBA16F scene target with depth, a documented fullscreen-pass hook, half-resolution bright-pass and separable Gaussian bloom, ACES tone mapping, exposure, and final sRGB conversion. `G` toggles bloom while retaining HDR and tone mapping. `Q` selects full-resolution HDR with eight blur iterations and high sphere detail, or 0.75-resolution HDR with four iterations and lower sphere detail. The existing lensing pass still consumes the HDR scene texture and sends its result through the bloom and tone-mapping composite. The gameplay/camera and black-hole modules are also implemented, including mission integration, event horizon, accretion disk, and body effects.

**Done (Earth and Saturn renderer):** Earth has a soft day/night terminator, night-side city lights, procedural ocean/land and cloud fallbacks, a separately rotating cloud shell, optional normal/specular maps, and an additive Fresnel atmosphere. Saturn has a tilted, alpha-blended ring with procedural band/gap fallback, Saturn shadowing the ring, and the ring shadowing Saturn. All Earth layers and the ring follow the body's black-hole transform and fade. Optional textures are read from `assets/textures/`; no texture files are included, and the app uses procedural fallbacks.

**Done (orbit visibility and selection):** Thin, translucent orbit guides render before opaque bodies so each planet's silhouette covers the guide across its projected disk. The selected body's ecliptic ring is centered on its current transformed position and scaled with its radius and black-hole deformation; the depth buffer hides the far part of the ring.

**Partial:** `I` changes HUD state only; instancing is not implemented. The old timer threat remains as an explicitly marked fallback alternative; the real black hole is selected by default. Scene sizes and distances are stylized, and the lensing/spaghettification are illustrative screen-space and transform effects rather than a physical simulation.

**Missing:** No requested HDR, bloom, tone-mapping, G, or Q items are missing. Decorative planetary rings and instancing are outside this module. Visual verification of the final orbit and selection changes remains partial because the rebuilt app did not expose a targetable window in this environment.

Build uses CMake FetchContent and requires dependency sources at configure time. A successful clean-source verification is reported with the project handoff, not assumed from this README.
