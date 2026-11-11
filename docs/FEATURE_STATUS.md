# Solar System 3D: Feature Status (internal checklist, not an official grade)

**Team:** Yernur Amanturlly, Renat Rat, Azamat Ziadanov | **Group:** IT-2508

**Legend:** DONE = implemented and built; code-verified = proven by code/build; visual pending = needs a visual check by the team; Optional = by design, not included.

## RENDERING

1. **DONE — code-verified.** HDR pipeline (RGBA16F), bloom, Gaussian blur, ACES tone mapping, sRGB. Evidence: `src/PostProcess.cpp:89-110,217-220`; `shaders/composite.frag:1-38`.
2. **DONE — code-verified.** Bloom toggle (G), High/Low quality (Q). Evidence: `src/Application.cpp:1689-1694,1733-1752`.
3. **DONE — code-verified; visual pending.** Gravitational lensing with L toggle and an automatic LENSING DEMO after the intro. Evidence: `src/Application.cpp:53,1648-1665,1963-1975,2178-2187`; `src/BlackHole.h:10,50-52`.
4. **DONE — code-verified; visual pending.** Procedural Sun (FBM) and procedural surfaces for the 8 planets and Moon. Evidence: `shaders/sun.frag:1-37`; `shaders/planet.frag:45-110`; `src/Application.cpp:660-665`.
5. **DONE — code-verified; visual pending.** Earth has procedural continents/biomes, clouds, atmosphere, city lights and ocean specular; the Moon is rendered. Evidence: `shaders/earth.frag:45-131`; `src/Application.cpp:1380-1405`.
6. **DONE — code-verified; visual pending.** Saturn has tilted rings with mutual shadows. Evidence: `src/Application.cpp:1320-1345`; `shaders/ring.frag:1-92`; `shaders/planet.frag:120-140`.
7. **DONE — code-verified; visual pending.** Asteroid belt with instancing (4,000 High / 1,800 Low), meteors (36 / 18), I and M toggles. Evidence: `src/Application.cpp:60-61,1439-1452,1568-1581,1741-1748`.
8. **DONE — code-verified; visual pending.** Window resize and minimized-window handling. Evidence: `src/Application.cpp:752,759-782,1935-1948`.

## GAMEPLAY

9. **DONE — code-verified; visual pending.** Mission mode has score, a 60-point target, threat, win/loss and restart. Evidence: `src/Application.h:136-139`; `src/Application.cpp:999-1048,1061-1070,1083-1114`; `src/BlackHole.cpp:116-147`.
10. **DONE — code-verified; visual pending.** Free Explore supports black-hole summon/return and a swallowed-Sun ending. Evidence: `src/Application.cpp:1051-1058,1754-1765,1770-1778`; `src/BlackHole.h:33-39`.
11. **DONE — code-verified; visual pending.** Black hole has an event horizon, accretion disk with Doppler effect, and body/asteroid effects. Evidence: `src/BlackHole.h:54-63`; `src/BlackHole.cpp:228-278`; `shaders/disk.frag:58-81`; `shaders/asteroid.vert:33-88`.
12. **DONE — code-verified.** Quiz questions and answer options are randomized; answers change threat speed. Evidence: `src/QuizRandomizer.cpp:39-52,74-110`; `src/Application.cpp:992-996,1099-1114`.
13. **DONE — code-verified; visual pending.** Milky Way view is available with Y and is a stylized visualization. Evidence: `src/Application.cpp:1723-1726`; `docs/REPORT_NOTES_RENDERER.md:124`.
14. **DONE — code-verified; visual pending.** Earth city map (P) with 50 offline cities and pins (N). Evidence: `src/Application.cpp:374-376,1704-1719,1721-1722`; `assets/cities.json` (50 records).

## INTERFACE AND AUDIO

15. **DONE — code-verified; visual pending.** Navy/cyan collapsible sidebar with flyout panels and an always-visible HUD showing team, mode, score and G/L/I/M/N/P/Q states. Evidence: `src/Application.cpp:54-59,2128-2187,2192-2225,2270-2290`; `src/UiTheme.h:10-12`. The HUD does not display K or asteroid draw-call count.
16. **DONE — code-verified.** Keyboard and mouse controls: Space pause; 1/2/3 speed; G bloom; L lensing; I instancing; M meteors; Q quality; B black-hole summon/return; R return/restart; F free-fly; [ / ] light rotation; Esc clear selection/return overview; F12 screenshot; Tab collapse sidebar; F1/H help; K mute; Y Milky Way; N pins; P Earth map; WASD move; Left Shift faster; left click selects; right drag orbits; wheel zooms. Evidence: `src/Application.cpp:1679-1805,1824-1864`; README.md `Controls` section.
17. **DONE — code-verified; visual pending.** Intro splash (5.2 s, skippable), help overlay (F1/H), screenshots (F12). Evidence: `src/Intro.h:7-8`; `src/Intro.cpp:57-75`; `src/Application.cpp:1768-1769,1780-1781,2711-2714`.
18. **DONE — code-verified; visual pending.** Synthesized audio, volume slider and mute (K); no audio files are required for generated ambience. Evidence: `src/SoundSynth.cpp:1-150`; `src/Audio.cpp:68-178`; `src/Application.cpp:1701-1703,2543-2555`. Audio output depends on build/runtime audio support.

## OPTIONAL (by design, not included)

19. **OPTIONAL — code-verified.** External planet and Earth texture images are optional; procedural fallbacks are used when image files are absent. Evidence: `src/Application.cpp:652-665`; `src/Texture.cpp:21-28`; `shaders/planet.frag:45-110`; `shaders/earth.frag:45-131`.

## DRAW-CALL COUNTER

20. **DONE — code-verified.** The asteroid draw-call counter is not shown to the user: `asteroidDrawCalls` is updated internally, but the HUD/sidebar drawing code never reads it. There is no place in the default or expanded sidebar where a user can find this count. Evidence: `src/Application.h:105`; `src/Application.cpp:1572-1581,2128-2187,2192-2225`.

## SUBMISSION STATUS (separate from features)

- Clean build from a fresh clone: **NOT VERIFIED**
- FPS table (bloom, quality, instancing, lensing on/off): **TODO**
- Report and 60-120 s video: **TODO**
- Topic-specific must-implement list (Section 6): **TODO, needs the topic sheet**

## Texture and font discovery notes

The optional sky panorama path is `assets/textures/stars_milky_way.jpg`; the procedural starfield/nebula is the fallback. Earth and planet image paths are optional and have procedural fallbacks. Evidence: `src/Application.cpp:652-665`; `shaders/sky.frag`; `src/Texture.cpp:21-28`.

Font discovery checks static Inter and JetBrains Mono fonts under `assets/fonts` first, then Segoe UI and Consolas from the Windows Fonts directory, then ImGui's built-in font. Evidence: `src/UiTheme.cpp:149-161,174-219`. Non-Windows builds skip the Windows system-font step.

**Visual checks:** all rendered appearance and layout claims above remain visual pending because the project window was not inspected in this audit.
