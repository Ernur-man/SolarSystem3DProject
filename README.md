# Event Horizon: Solar System Explorer

A C++17 / OpenGL 3.3 Core university graphics project. The scene includes a procedural starfield background and runs without image assets. An optional equirectangular sky texture can be placed at `assets/textures/stars_milky_way.jpg`.

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

The window can be resized and safely handles a minimized framebuffer. The splash credits and HUD use the same names in `UiTheme::kTeamMembers`. Inter supplies interface text; JetBrains Mono supplies measurements and readouts. Static TTF files are optional and use the SIL Open Font License. Font discovery is case-insensitive, variable fonts are rejected, and ImGui's size-matched default font is used with a warning when matching static files are unavailable. Fonts are rasterized at device pixel size and the atlas is rebuilt when display DPI changes; `FontGlobalScale` stays at 1.0. See `assets/fonts/README.txt` for file names and licence guidance.

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

**Done:** HDR rendering uses an RGBA16F scene target with depth, a documented fullscreen-pass hook, half-resolution bright-pass and separable Gaussian bloom, ACES tone mapping, exposure, and final sRGB conversion. `G` toggles bloom while retaining HDR and tone mapping. `Q` selects full-resolution HDR with eight blur iterations and high sphere detail, or 0.75-resolution HDR with four iterations and lower sphere detail. The existing lensing pass still consumes the HDR scene texture and sends its result through the bloom and tone-mapping composite. The gameplay/camera and black-hole modules are also implemented, including mission integration, event horizon, accretion disk, and body effects.

**Done (planet surfaces, Earth, Moon, and Saturn):** Mercury, Venus, Mars, Jupiter, Saturn, Uranus, Neptune, and the Moon have optional sRGB color maps with mipmaps and extension-supported anisotropic filtering; short procedural fallbacks render when maps are absent. Earth's separate texture pipeline is unchanged. Earth has one rendered, tidally locked Moon with a 1/12-year stylized orbit and matching black-hole body effects. Saturn's 27-degree equatorial tilt is applied before its local-Y surface spin; its ring stays in the fixed tilted equatorial plane while angular dust/band patterns rotate at simulation time (about 25 simulated seconds per inner-edge revolution at 1x, slower outward). The reciprocal Saturn/ring analytic shadows remain on the fixed plane.

**Done (resize, meteors, asteroid belt):** Framebuffer changes resize the existing HDR/bloom and BlackHole lensing targets. ImGui uses logical window dimensions and picking maps logical cursor coordinates to framebuffer pixels. M toggles HDR-scene meteors (36 High / 18 Low). The belt has 4,000 High / 1,800 Low asteroids, I switches instanced/individual draws, and the HUD reports draw calls. The three-radius Mars/Jupiter margins cross in this stylized scale (8.0 > 6.95), so the user-selected available gap uses 1.5R clearances (7.55–8.225) and ±0.20 vertical spread. Asteroids keep planet repulsion and gain stateless black-hole pull, horizon fade, and HDR heat flashes; meteors use the same pure pull/fade behavior.

**Done (Earth procedural fallback):** FBM generates connected landmasses, coastal water, latitude/height biomes, snow and ice. Ocean specular and land-masked city lights coexist with the optional textures; the separately rotating cloud shell and Fresnel atmosphere remain.

**Done (procedural planet fallback):** Mercury/Moon use cellular crater bowls and rims; Venus uses warped cloud bands; Mars adds rust regions and polar caps; Jupiter has turbulent bands and a Great Red Spot; Saturn has fine pale-gold bands; Uranus has faint cyan bands; Neptune has sparse blue streaks. UV patterns rotate with their sphere models. Both the Sun at the origin and the movable point light illuminate the surfaces.

**Done (black-hole gameplay):** The 90-second-at-1x threat can be returned along the same eased progress curve. Mission loss offers Restart and Back to menu; Free Explore offers a swallowed-Sun ending with Restart or Keep watching, and R restarts after swallowing. Asteroids and meteors follow the black hole without accumulated drift; asteroid-versus-planet repulsion remains active.

**Done (Milky Way view):** Free Explore has a Solar System / Milky Way view toggle and Y shortcut while the black hole is Idle. Smooth simulation-time transitions render bounded, glowing camera-facing trails and a subtle starfield parallax; physics and picking remain Sun-centred. Trails pause with Space, scale with simulation speed, and clear on restart. This view is a stylized visualization.

**Added (Milky Way camera):** The Milky Way overview applies `kMilkyWayCameraYawDeg = -90.0f` to the normal overview azimuth. Its two-second smoothstep transition uses the same direction blend as the mode and returns to Solar System orientation when the mode is turned off. Restart resets the angle to the normal overview. With the camera formula below, negative yaw rotates the horizontal camera offset toward the camera's prior left side.

**Added (sound):** Synthesized 9.5-second stereo loops supply distinct world tones and filtered noise, plus deep-space ambience. Gains follow focus/proximity and body opacity, and the HUD provides volume and mute controls. See the Gameplay Notes equations for the synthesis and distance fade.

**Done (interface):** the ImGui UI is a flat scientific instrument: square corners, hairline separators, aligned mono measurements, one amber accent and red reserved for danger or incorrect answers. Its 24%-width right dock is scrollable, with an overview table, selected-world datasheet, simulation controls, technique rows, audio control, mission status, and controls. Centered quiz, mission, ending, and help surfaces use matching hairlines and typography. `UiTheme::kUiTextScale` defaults to 1.2; font rasterization includes content DPI and the atlas rebuilds when that scale changes, while `FontGlobalScale` remains 1.0. Optional static Inter and JetBrains Mono fonts use the SIL Open Font License; case-insensitive discovery rejects variable TTFs and falls back to size-matched built-in fonts. See `assets/fonts/README.txt` for expected names and licence guidance. Course identity, score, and toggle states remain visible in the fixed panel header.

**Done (intro splash):** a 5.2-second, once-per-launch overlay reveals the live HDR, bloom, and tone-mapped scene from black, then fades the title, line, and shared team credit. It uses a frame-rate-independent smootherstep timeline, clamps frame delta, fades on any key or mouse click, and hides the window until the first completed frame has been swapped. `--no-intro` skips directly to the existing Start screen. Missing optional fonts warn and fall back to ImGui's default font.

**Done (quiz randomization):** each world uses a Fisher–Yates question bag so questions are used once before the bag refills, with the previous question kept off the next bag's first slot. Answer options reshuffle by their answer text identity for each display. `kQuizFixedSeed` is in `src/QuizRandomizer.h`: zero uses `std::random_device` with a clock fallback; a nonzero value gives a repeatable seed sequence. Mission and quiz starts print the seed.

**Partial:** Visual checks for resizing/DPI, Earth, quiz/Q, lensing (B/L), and numeric performance across instanced/non-instanced consuming modes remain to be done. The old timer threat remains as a marked fallback; scene scales and black-hole effects are illustrative rather than physically simulated.

**Missing:** No requested renderer feature remains unimplemented. Optional Earth texture images are not included.

Build uses CMake FetchContent and requires dependency sources at configure time. A successful clean-source verification is reported with the project handoff, not assumed from this README.
