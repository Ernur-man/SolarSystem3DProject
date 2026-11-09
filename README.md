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

GLFW (Zlib/libpng), GLAD (MIT), GLM (Happy Bunny and MIT), Dear ImGui (MIT), stb (public domain / MIT). Dependencies are fetched at configure time. stb_image loads the optional sky texture. The interface can load Inter and Space Grotesk fonts from `assets/fonts/`; both font families use the SIL Open Font License. Font files are optional and are not included; see `assets/fonts/README.txt`.

## Controls

- `Space`: pause or resume simulation; `1` / `2` / `3`: simulation speed 1x / 5x / 10x.
- `G`: toggle bloom rendering (the HDR and tone-mapping path remains active); `L`: toggle screen-space gravitational lensing; `I`: toggle asteroid instancing; `M`: toggle meteors.
- `Q`: switch High / Low quality. High uses full-resolution HDR, eight blur iterations, and 32 x 48 sphere segments; Low uses 0.75-resolution HDR, four blur iterations, and 8 x 12 sphere segments.
- `B`: summon or return the black hole; `R`: return it in Free Explore, restart Free Explore after the Sun is swallowed, or restart an active/finished mission; `F`: toggle free-fly; `[` / `]`: rotate the Sun light direction.
- `Esc`: clear selection and smoothly return to the overview; `F12`: save a PNG in `screenshots/`.
- `F1` or `H`: show or hide the complete controls overlay.
- `N`: toggle Earth city pins and labels. Select Earth and press `P` to open or close its offline map panel.
- The F1 help also notes that optional planet maps have procedural fallbacks and Earth has a rendered Moon.
- Earth map mode includes 50 offline city markers, lat/lon picking, and a pannable, zoomable equirectangular map. Without `assets/textures/earth_day.jpg`, the map is generated at startup from the Earth procedural shader recipe; it is not live map imagery.
- Left click or select a name in Mission Control: select a planet or the Sun and fly to it. Right mouse drag rotates the camera; the wheel zooms.
- In free-fly, `W` / `A` / `S` / `D` move and `Shift` moves faster.
- The right-side dark space interface has clickable state chips that stay synchronized with `G`, `L`, `I`, `M`, and `Q`; expand **Controls** at the bottom for the complete key list. `F1` or `H` opens the same help in a centered card.
- The start screen offers **Start Mission** and **Free Explore**. Mission visits and correct quiz answers earn points while the threat advances; quiz questions pause simulation time.

The window can be resized and safely handles a minimized framebuffer. Team name is a placeholder.

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

**Done (interface):** the ImGui panel uses a near-black blue palette, warm yellow accent, a SOLAR header, simulation hero, status chips, clickable world cards, aligned information rows, and matching centered mission, ending, quiz, and help cards. `UiTheme::kUiTextScale` defaults to 1.2 and scales the font atlas pixel sizes and layout measurements with GLFW content scale; `FontGlobalScale` is not stacked on top. Layout uses logical window dimensions with a 26%-width panel capped at 460 px and 40% of the window. The requested Inter and Space Grotesk font files load at DPI-scaled sizes when present; missing files use ImGui's default font and print a warning. The status strip remains pinned above the scrollable world/info content.

**Done (quiz randomization):** each world uses a Fisher–Yates question bag so questions are used once before the bag refills, with the previous question kept off the next bag's first slot. Answer options reshuffle by their answer text identity for each display. `kQuizFixedSeed` is in `src/QuizRandomizer.h`: zero uses `std::random_device` with a clock fallback; a nonzero value gives a repeatable seed sequence. Mission and quiz starts print the seed.

**Partial:** Visual checks for resizing/DPI, Earth, quiz/Q, lensing (B/L), and numeric performance across instanced/non-instanced consuming modes remain to be done. The old timer threat remains as a marked fallback; scene scales and black-hole effects are illustrative rather than physically simulated.

**Missing:** No requested renderer feature remains unimplemented. Optional Earth texture images are not included.

Build uses CMake FetchContent and requires dependency sources at configure time. A successful clean-source verification is reported with the project handoff, not assumed from this README.
