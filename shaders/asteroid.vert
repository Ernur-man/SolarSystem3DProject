#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec4 instanceColumn0;
layout(location = 4) in vec4 instanceColumn1;
layout(location = 5) in vec4 instanceColumn2;
layout(location = 6) in vec4 instanceColumn3;
layout(location = 7) in vec4 instanceColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int useInstance;
uniform int planetCount;
uniform vec3 planetPositions[9];
uniform float planetRadii[9];
uniform vec3 baseColor;
uniform vec3 holePosition;
uniform float progress;
uniform float horizonRadius;
uniform float pullRadius;
uniform float maximumPullStrength;
uniform float maximumBodyStretch;
uniform float stretchInfluenceRadius;
uniform float activationThreshold;

out vec3 vNormal;
out vec3 vWorldPosition;
out vec3 vColor;
out float vHeat;
out float vOpacity;

void applyHoleEffect(inout vec3 worldPoint, vec3 objectCenter, out float opacity, out float heat) {
    // Match BlackHole::affectBody: activation, inverse-square-style pull, axial stretch, horizon fade.
    float activation = smoothstep(activationThreshold, 1.0, progress);
    vec3 towardHole = holePosition - objectCenter;
    float distanceToHole = length(towardHole);
    float pullStrength = maximumPullStrength * activation;
    float pullWeight = clamp(pullStrength /
                             (1.0 + (distanceToHole / pullRadius) *
                                        (distanceToHole / pullRadius)), 0.0, 1.0);
    vec3 displacedCenter = objectCenter + towardHole * pullWeight;
    vec3 axisVector = holePosition - displacedCenter;
    float distanceAfterPull = length(axisVector);
    vec3 axis = distanceAfterPull > 0.00001 ? axisVector / distanceAfterPull
                                             : vec3(0.0, 0.0, 1.0);
    float nearHole = 1.0 - smoothstep(horizonRadius, stretchInfluenceRadius, distanceAfterPull);
    float stretch = 1.0 + maximumBodyStretch * nearHole * activation;
    float horizonFade = 1.0 - smoothstep(horizonRadius * 0.65, horizonRadius * 1.4,
                                         distanceAfterPull);
    float swallowedAmount = clamp(horizonFade * activation, 0.0, 1.0);
    opacity = 1.0 - swallowedAmount;
    // A short HDR flash peaks as the horizon fade begins, then drops before the object vanishes.
    heat = smoothstep(0.03, 0.55, swallowedAmount) *
           (1.0 - smoothstep(0.78, 0.98, swallowedAmount));

    vec3 offset = worldPoint - objectCenter;
    vec3 parallel = axis * dot(offset, axis);
    vec3 perpendicular = offset - parallel;
    worldPoint = displacedCenter +
                 (parallel * stretch + perpendicular / sqrt(stretch)) * opacity;
}

void main() {
    mat4 transform = useInstance != 0
                         ? mat4(instanceColumn0, instanceColumn1, instanceColumn2, instanceColumn3)
                         : model;
    vec4 world = transform * vec4(aPosition, 1.0);
    vec3 asteroidCenter = (transform * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    float opacity;
    float heat = 0.0;
    applyHoleEffect(world.xyz, asteroidCenter, opacity, heat);
    for (int i = 0; i < planetCount; ++i) {
        vec3 away = world.xyz - planetPositions[i];
        float distanceToBody = length(away);
        float influenceRadius = 1.5 * planetRadii[i];
        if (distanceToBody < influenceRadius && distanceToBody > 0.0001) {
            float falloff = 1.0 - smoothstep(planetRadii[i], influenceRadius, distanceToBody);
            // Smoothly push vertices outward inside 1.5 radii to prevent visible planet overlap.
            world.xyz += normalize(away) * (influenceRadius - distanceToBody) * falloff;
            heat = max(heat, 1.0 - smoothstep(planetRadii[i], influenceRadius, distanceToBody));
        }
    }
    vNormal = normalize(mat3(transpose(inverse(transform))) * aNormal);
    vWorldPosition = world.xyz;
    vColor = useInstance != 0 ? instanceColor.rgb : baseColor;
    vHeat = heat;
    vOpacity = opacity;
    gl_Position = projection * view * world;
}
