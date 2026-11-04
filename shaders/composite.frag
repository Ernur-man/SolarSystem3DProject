#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform bool bloomEnabled;
uniform float exposure;

vec3 acesFit(vec3 color) {
    // ACES fitted tone curve maps exposed linear HDR values into the display range [0,1].
    return clamp((color * (2.51 * color + 0.03)) /
                     (color * (2.43 * color + 0.59) + 0.14),
                 0.0, 1.0);
}

vec3 linearToSrgb(vec3 color) {
    // The piecewise sRGB OETF converts the tone-mapped linear display value for the monitor.
    vec3 low = 12.92 * color;
    vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), color));
}

void main() {
    vec3 hdrColor = texture(sceneTexture, uv).rgb;
    if (bloomEnabled)
        hdrColor += texture(bloomTexture, uv).rgb;
    vec3 mapped = acesFit(hdrColor * exposure);
    FragColor = vec4(linearToSrgb(mapped), 1.0);
}
