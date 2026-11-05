#version 330 core
layout(location = 0) in vec3 aPosition;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform int drawTail;
uniform vec3 center;
uniform vec3 tailDirection;
uniform vec3 tailSide;
uniform float tailLength;
uniform float tailWidth;
uniform vec3 holePosition;
uniform float progress;
uniform float horizonRadius;
uniform float pullRadius;
uniform float maximumPullStrength;
uniform float maximumBodyStretch;
uniform float stretchInfluenceRadius;
uniform float activationThreshold;

out vec2 vUv;
out float vTail;
out float vOpacity;
out float vHeat;

void applyHoleEffect(inout vec3 worldPoint, vec3 objectCenter, out float opacity, out float heat) {
    // Use the same stateless pull, stretch, and horizon fade as the planet body transform.
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
    heat = smoothstep(0.03, 0.55, swallowedAmount) *
           (1.0 - smoothstep(0.78, 0.98, swallowedAmount));
    vec3 offset = worldPoint - objectCenter;
    vec3 parallel = axis * dot(offset, axis);
    vec3 perpendicular = offset - parallel;
    worldPoint = displacedCenter +
                 (parallel * stretch + perpendicular / sqrt(stretch)) * opacity;
}

void main() {
    vUv = aPosition.xy * 0.5 + 0.5;
    vTail = float(drawTail);
    vec3 worldPosition;
    if (drawTail != 0) {
        // A camera-facing strip stretches behind the meteor along its travel path.
        worldPosition = center + tailSide * (aPosition.x * tailWidth) +
                        tailDirection * (aPosition.y * tailLength);
    } else {
        worldPosition = (model * vec4(aPosition, 1.0)).xyz;
    }
    applyHoleEffect(worldPosition, center, vOpacity, vHeat);
    gl_Position = projection * view * vec4(worldPosition, 1.0);
}
