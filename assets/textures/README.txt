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

Missing Earth maps, the Saturn ring map, and the sky map each produce a warning naming the path;
the shader uses its procedural fallback. A Mars normal map is not currently present, so Mars uses
its geometric sphere normals.

Solar System Scope textures are CC BY 4.0 and require attribution:
https://www.solarsystemscope.com/textures/
