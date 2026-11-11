# Event Horizon: Solar System Explorer

A C++17 / OpenGL 3.3 Core university graphics project. Without the optional `assets/textures/stars_milky_way.jpg` panorama, the sky uses a procedural, direction-based nebula with clumped gas, dust lanes, and layered depth; no image asset is required.

## Build and run (Windows 10/11, VS2022)

Install Visual Studio Community 2022 with **Desktop development with C++**, CMake, Git, and a working internet connection (CMake FetchContent downloads dependencies). In Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\solar_system.exe
```

The same configure/build commands work with GCC or Clang. CMake downloads pinned dependency sources at configure time; runtime requires no network access. In Visual Studio use **File → Open → Folder** and select this directory, or open the generated solution in `build`.

## Libraries

GLFW (Zlib/libpng), GLAD (MIT), GLM (Happy Bunny and MIT), Dear ImGui (MIT), stb (public domain / MIT). Dependencies are fetched at configure time. stb_image loads the optional sky texture. Optional static Inter and JetBrains Mono TTF files in `assets/fonts/` use the SIL Open Font License; keep matching license/attribution files with font binaries. Expected names and fallback behavior are documented in `assets/fonts/README.txt`.

## Sound

World ambience is synthesized in C++ at startup, so the game needs no audio assets. The high-level miniaudio engine uses the bundled single header at `third_party/miniaudio/miniaudio.h`, version `0.11.22` (public domain / MIT-0); it is not downloaded by CMake. If the header is absent, CMake builds a silent audio stub and the HUD shows `Sound: OFF (not built)`. If the output device cannot start, the game continues with sound disabled. The initial master volume is 35%; adjust it in the Mission Control panel or mute with `K`. Selecting a world plays a quiet synthesized blip. The overview uses a very quiet synthesized deep-space loop. Optional world recordings can override the generated loops; names, supported formats, and licence guidance are in `assets/audio/README.txt`.

## Controls

- `Space`: pause or resume simulation; `1` / `2` / `3`: simulation speed 1x / 5x / 10x.
- `G`: toggle bloom rendering (the HDR and tone-mapping path remains active); `L`: toggle screen-space gravitational lensing; `I`: toggle asteroid instancing; `M`: toggle meteors.
- `Q`: switch High / Low quality. High uses full-resolution HDR, eight blur iterations, and 32 x 48 sphere segments; Low uses 0.75-resolution HDR, four blur iterations, and 8 x 12 sphere segments.
- `B`: summon or return the black hole; `R`: return it in Free Explore, restart Free Explore after the Sun is swallowed, or restart an active/finished mission; `F`: toggle free-fly; `[` / `]`: rotate the Sun light direction.
- `Esc`: clear selection and smoothly return to the overview; `F12`: save a PNG in `screenshots/`.
- Startup shows a 5.2-second scene intro once per launch; any key or mouse click skips it with a short fade. Pass `--no-intro` to start directly at the menu.
- `Tab`: collapse or expand the right sidebar.
- After the intro, the Start screen runs a 7-second automatic black-hole “LENSING DEMO” so gravitational lensing is visible within the first seconds; any key or mouse click skips the demo. Mission behaviour is unchanged.
- `F1` or `H`: show or hide the complete controls overlay.
- `K`: smoothly mute or unmute synthesized ambience. Mission Control has a master-volume slider.
- `Y`: toggle the Solar System / Milky Way view in Free Explore while the black hole is idle.
- `N`: toggle Earth city pins and labels. Select Earth and press `P` to open or close its offline map panel.
- The F1 help also notes that optional planet maps have procedural fallbacks and Earth has a rendered Moon.
- Earth map mode includes 50 offline city markers, lat/lon picking, and a pannable, zoomable equirectangular map. Without `assets/textures/earth_day.jpg`, the map is generated at startup from the Earth procedural shader recipe; it is not live map imagery.
- Left click or select a name in Mission Control: select a planet or the Sun and fly to it. Right mouse drag rotates the camera; the wheel zooms.
- In free-fly, `W` / `A` / `S` / `D` move and `Left Shift` moves faster.
- The right-side scientific instrument panel has a scrollable world table/data sheet, simulation controls, technique rows, sound control, and mission status. Technique rows stay synchronized with `G`, `L`, `I`, `M`, `N`, `P`, `Q`, and `K`. Expand **Controls** at the bottom for the key map. `F1` or `H` opens the same controls in a centered two-column table.
- The start screen offers **Start Mission** and **Free Explore**. Mission visits and correct quiz answers earn points while the threat advances; quiz questions pause simulation time.

The window can be resized and safely handles a minimized framebuffer. The splash credits and HUD use the same names in `UiTheme::kTeamMembers`. Font discovery first checks `assets/fonts` for static Inter and JetBrains Mono TTFs (case-insensitive; variable fonts are skipped), then on Windows reads Segoe UI and Consolas from the system Fonts directory, then falls back to ImGui's built-in font with a warning. Font files are read at runtime; no font binaries are committed. Fonts are rasterized at DPI-aware pixel size and the atlas is rebuilt when display DPI changes; `FontGlobalScale` stays at 1.0. See `assets/fonts/README.txt` for optional project font file names and licence guidance.

## Team and modules

| Member | Module they present and can explain at the defence |
|---|---|
| Ernur | Gameplay, UI, camera, integration, Sun FBM shader, picking, quiz |
| Renat | Renderer (HDR, bloom, Earth, rings, instancing) |
| Azamat | Black hole (state machine, lensing, disk, Doppler) |

## AI assistance

Code was developed with OpenAI Codex under Ernur's direction, using prompts prepared with Claude. The team reviews AI-generated code; each member explains and can modify their shaders at the defence.

## Data and image provenance

`assets/planets.json` and `assets/quiz.json` contain educational facts and quiz questions. Planetary values are approximate and checked against NASA's [Planetary Fact Sheet - Metric](https://nssdc.gsfc.nasa.gov/planetary/factsheet/) and [Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html?level=1). The moon counts reflect that fact sheet's March 2025 update. No texture files are included. Optional maps are read from `assets/textures/`; absent files use procedural surfaces. Solar System Scope planet, Moon, and ring textures are licensed CC BY 4.0 and require attribution: [Solar System Scope Textures](https://www.solarsystemscope.com/textures/). **Credit: Textures by Solar System Scope, licensed CC BY 4.0; attribution is required when using them.** Texture images are not downloaded automatically and must not be committed.

## Feature status

**Team:** Yernur Amanturlly, Renat Rat, Azamat Ziadanov | **Group:** IT-2508

**Legend:** DONE = implemented and built; code-verified = proven by code/build; visual pending = needs a visual check by the team; Optional = by design, not included.

### RENDERING

1. **DONE — code-verified.** HDR pipeline (RGBA16F), bloom, Gaussian blur, ACES tone mapping, sRGB.
2. **DONE — code-verified.** Bloom toggle (G), High/Low quality (Q).
3. **DONE — code-verified; visual pending.** Gravitational lensing with L toggle and an automatic LENSING DEMO after the intro.
4. **DONE — code-verified; visual pending.** Procedural Sun (FBM) and procedural surfaces for the 8 planets and Moon.
5. **DONE — code-verified; visual pending.** Earth has procedural continents/biomes, clouds, atmosphere, city lights and ocean specular; the Moon is rendered.
6. **DONE — code-verified; visual pending.** Saturn has tilted rings with mutual shadows.
7. **DONE — code-verified; visual pending.** Asteroid belt with instancing (4,000 High / 1,800 Low), meteors (36 / 18), I and M toggles.
8. **DONE — code-verified; visual pending.** Window resize and minimized-window handling.

### GAMEPLAY

9. **DONE — code-verified; visual pending.** Mission mode has score, a 60-point target, threat, win/loss and restart.
10. **DONE — code-verified; visual pending.** Free Explore supports black-hole summon/return and a swallowed-Sun ending.
11. **DONE — code-verified; visual pending.** Black hole has an event horizon, accretion disk with Doppler effect, and body/asteroid effects.
12. **DONE — code-verified.** Quiz questions and answer options are randomized; answers change the threat speed.
13. **DONE — code-verified; visual pending.** Milky Way view is available with Y and is a stylized visualization.
14. **DONE — code-verified; visual pending.** Earth city map (P) with 50 offline cities and pins (N).

### INTERFACE AND AUDIO

15. **DONE — code-verified; visual pending.** Navy/cyan collapsible sidebar with flyout panels and an always-visible HUD showing team, mode, score and G/L/I/M/N/P/Q states. The HUD does not display K or asteroid draw-call count.
16. **DONE — code-verified.** Keyboard and mouse controls: Space pause; 1/2/3 speed; G bloom; L lensing; I instancing; M meteors; Q quality; B black-hole summon/return; R return/restart; F free-fly; [ / ] light rotation; Esc clear selection/return overview; F12 screenshot; Tab collapse sidebar; F1/H help; K mute; Y Milky Way; N pins; P Earth map; WASD move; Left Shift faster; left click selects; right drag orbits; wheel zooms.
17. **DONE — code-verified; visual pending.** Intro splash (5.2 s, skippable), help overlay (F1/H), screenshots (F12).
18. **DONE — code-verified; visual pending.** Synthesized audio, volume slider and mute (K); no audio files are required for generated ambience. Audio output depends on build/runtime audio support.

### OPTIONAL (by design, not included)

19. **OPTIONAL — code-verified.** External planet and Earth texture images are optional; procedural fallbacks are used when image files are absent.

### DRAW-CALL COUNTER

20. **DONE — code-verified.** The asteroid draw-call counter is not shown to the user. It is updated internally, but the HUD and sidebar do not read it; there is no place in either default or expanded sidebar where the count can be found.

### SUBMISSION STATUS (separate from features)

- Clean build from a fresh clone: **NOT VERIFIED**
- FPS table (bloom, quality, instancing, lensing on/off): **TODO**
- Report and 60-120 s video: **TODO**
- Topic-specific must-implement list (Section 6): **TODO, needs the topic sheet**

For exact source evidence, texture/font discovery notes, and audit references, see [docs/FEATURE_STATUS.md](docs/FEATURE_STATUS.md).
## Offline / clean build notes

Configuring a clean checkout requires internet: CMake FetchContent downloads GLFW, GLM, GLAD, Dear ImGui, and stb. By default, GLAD also downloads the Khronos `gl.xml` specification during build-time code generation. The running application requires no network connection.
