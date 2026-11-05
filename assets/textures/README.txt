Optional textures are loaded from this folder. The application runs without any of them.
Do not commit texture files.

Expected files:
    stars_milky_way.jpg   equirectangular sky background
    earth_day.jpg         Earth daytime/albedo map
    earth_night.jpg       Earth city-light map
    earth_clouds.jpg      Earth cloud coverage map
    earth_normal.jpg      Earth tangent-space normal map
    earth_specular.jpg    Earth specular mask
    saturn_ring.png       Saturn ring color and alpha map
    mercury.jpg           Mercury color/albedo map
    venus.jpg             Venus color/cloud map
    mars.jpg              Mars color/albedo map
    jupiter.jpg           Jupiter color/cloud map
    saturn.jpg            Saturn color/cloud map
    uranus.jpg            Uranus color/cloud map
    neptune.jpg           Neptune color/cloud map
    moon.jpg              Moon color/albedo map

Missing image maps produce a warning naming the path; planet and Moon shaders use their procedural
surface fallback. The application does not download or require any image files.

Solar System Scope textures are CC BY 4.0 and require attribution:
https://www.solarsystemscope.com/textures/
Credit: Textures by Solar System Scope, licensed CC BY 4.0; attribution is required when using them.
