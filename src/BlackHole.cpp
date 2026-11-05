#include "BlackHole.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

namespace {
float smoothUnit(float value) {
    const float x = std::clamp(value, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float smoothBetween(float edge0, float edge1, float value) {
    return smoothUnit((value - edge0) / (edge1 - edge0));
}
}

bool BlackHole::initialize(int width, int height) {
    if (!horizonShader.load("shaders/blackhole.vert", "shaders/blackhole.frag") ||
        !diskShader.load("shaders/disk.vert", "shaders/disk.frag") ||
        !lensingShader.load("shaders/lensing.vert", "shaders/lensing.frag")) {
        release();
        return false;
    }

    horizonSphere.makeSphere(32, 48);
    accretionDisk.makeRing(DISK_INNER_RADIUS, DISK_OUTER_RADIUS);
    glGenVertexArrays(1, &fullscreenVao);
    initialized = true;
    if (!resize(width, height)) {
        release();
        return false;
    }
    return true;
}

bool BlackHole::resize(int width, int height) {
    if (!initialized || width <= 0 || height <= 0)
        return false;
    if (targetWidth == width && targetHeight == height && lensingFramebuffer != 0)
        return true;

    GLint oldDrawFramebuffer = 0;
    GLint oldReadFramebuffer = 0;
    GLint oldTexture = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    releaseLensingTarget();
    targetWidth = width;
    targetHeight = height;
    const bool complete = createLensingTarget();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(oldReadFramebuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    if (!complete) {
        releaseLensingTarget();
        targetWidth = 0;
        targetHeight = 0;
    }
    return complete;
}

bool BlackHole::createLensingTarget() {
    glGenFramebuffers(1, &lensingFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, lensingFramebuffer);
    glGenTextures(1, &lensingTexture);
    glBindTexture(GL_TEXTURE_2D, lensingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, targetWidth, targetHeight, 0, GL_RGBA, GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lensingTexture, 0);
    return checkLensingFramebuffer();
}

void BlackHole::releaseLensingTarget() {
    if (lensingTexture != 0)
        glDeleteTextures(1, &lensingTexture);
    if (lensingFramebuffer != 0)
        glDeleteFramebuffers(1, &lensingFramebuffer);
    lensingTexture = 0;
    lensingFramebuffer = 0;
}

bool BlackHole::checkLensingFramebuffer() const {
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;
    std::cerr << "Framebuffer incomplete [BlackHole lensing output FBO]: 0x" << std::hex << status
              << std::dec << '\n';
    return false;
}

void BlackHole::release() {
    releaseLensingTarget();
    if (fullscreenVao != 0)
        glDeleteVertexArrays(1, &fullscreenVao);
    if (horizonShader.id != 0)
        glDeleteProgram(horizonShader.id);
    if (diskShader.id != 0)
        glDeleteProgram(diskShader.id);
    if (lensingShader.id != 0)
        glDeleteProgram(lensingShader.id);
    fullscreenVao = 0;
    horizonShader.id = 0;
    diskShader.id = 0;
    lensingShader.id = 0;
    initialized = false;
}

void BlackHole::setThreatSpeedMultiplier(float multiplier) {
    targetSpeedMultiplier = std::clamp(multiplier, 0.25f, 4.0f);
}

float BlackHole::getThreatProgress() const {
    return progress;
}

bool BlackHole::isSunSwallowed() const {
    return progress >= 1.0f;
}

void BlackHole::update(float deltaTime) {
    if (deltaTime <= 0.0f)
        return;
    const float easing = 1.0f - std::exp(-3.0f * deltaTime);
    speedMultiplier += (targetSpeedMultiplier - speedMultiplier) * easing;
    const float step = deltaTime * speedMultiplier / kSwallowSecondsAtOneX;
    if (state == BlackHoleState::Approaching || state == BlackHoleState::Consuming) {
        progress = std::min(progress + step, 1.0f);
        if (progress >= 1.0f)
            setState(BlackHoleState::Consuming);
    } else if (state == BlackHoleState::Returning) {
        progress = std::max(progress - step, 0.0f);
        if (progress <= 0.0f)
            setState(BlackHoleState::Idle);
    }
}

void BlackHole::reset() {
    progress = 0.0f;
    speedMultiplier = 1.0f;
    targetSpeedMultiplier = 1.0f;
    setState(BlackHoleState::Idle);
}

void BlackHole::summon() {
    if (state == BlackHoleState::Idle)
        setState(BlackHoleState::Approaching);
}

void BlackHole::returnBack() {
    if (state == BlackHoleState::Approaching || state == BlackHoleState::Consuming)
        setState(BlackHoleState::Returning);
}

bool BlackHole::isIdle() const {
    return state == BlackHoleState::Idle;
}

BlackHoleState BlackHole::getState() const {
    return state;
}

const char *BlackHole::getStateName() const {
    switch (state) {
    case BlackHoleState::Idle:
        return "Idle";
    case BlackHoleState::Approaching:
        return "Approaching";
    case BlackHoleState::Consuming:
        return "Consuming";
    case BlackHoleState::Returning:
        return "Returning";
    }
    return "Unknown";
}

void BlackHole::setState(BlackHoleState nextState) {
    if (state == nextState)
        return;
    state = nextState;
    std::cout << "Black hole state: " << getStateName() << '\n';
}

glm::vec3 BlackHole::getPosition() const {
    const glm::vec3 farPosition(6.0f, 11.0f, 18.0f);
    const float easedProgress = smoothUnit(progress);
    return glm::mix(farPosition, glm::vec3(0.0f), easedProgress);
}

float BlackHole::getVisibility() const {
    return smoothUnit(progress / 0.12f);
}

float BlackHole::getHorizonRadius() const {
    return EVENT_HORIZON_RADIUS;
}

float BlackHole::getPullRadius() const {
    return PULL_RADIUS;
}

float BlackHole::getMaxPullStrength() const {
    return MAX_PULL_STRENGTH;
}

float BlackHole::getMaxBodyStretch() const {
    return MAX_BODY_STRETCH;
}

float BlackHole::getStretchInfluenceRadius() const {
    return DISK_OUTER_RADIUS;
}

float BlackHole::getBodyActivationThreshold() const {
    return BODY_ACTIVATION_THRESHOLD;
}

BodyEffect BlackHole::affectBody(const glm::vec3 &orbitPosition, bool sun) const {
    BodyEffect result;
    result.position = orbitPosition;
    if (state == BlackHoleState::Idle || progress <= 0.0f)
        return result;

    const glm::vec3 holePosition = getPosition();
    const glm::vec3 towardHole = holePosition - orbitPosition;
    const float distance = glm::length(towardHole);

    const float threshold = sun ? 0.92f : BODY_ACTIVATION_THRESHOLD;
    const float activation = smoothBetween(threshold, 1.0f, progress);
    const float pullStrength = MAX_PULL_STRENGTH * activation;
    const float pullWeight =
        std::clamp(pullStrength / (1.0f + (distance / PULL_RADIUS) * (distance / PULL_RADIUS)),
                   0.0f, 1.0f);
    result.position = orbitPosition + towardHole * pullWeight;

    const glm::vec3 postPullDirection = holePosition - result.position;
    const float postPullDistance = glm::length(postPullDirection);
    const glm::vec3 axis = postPullDistance > 0.00001f
                               ? postPullDirection / postPullDistance
                               : glm::vec3(0.0f, 0.0f, 1.0f);
    const float nearHole = 1.0f - smoothBetween(EVENT_HORIZON_RADIUS,
                                                DISK_OUTER_RADIUS, postPullDistance);
    // Blend the stretch axis from world up into the hole direction as the effect activates.
    const float alignmentWeight = activation * nearHole;
    const glm::vec3 blendedAxis =
        glm::mix(glm::vec3(0.0f, 0.0f, 1.0f), axis, alignmentWeight);
    const glm::vec3 alignedAxis = glm::length(blendedAxis) > 0.00001f
                                      ? glm::normalize(blendedAxis)
                                      : axis;
    const glm::vec3 reference = std::abs(glm::dot(alignedAxis, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.95f
                                    ? glm::vec3(1.0f, 0.0f, 0.0f)
                                    : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(reference, alignedAxis));
    const glm::vec3 up = glm::normalize(glm::cross(alignedAxis, right));
    result.basis[0] = right;
    result.basis[1] = up;
    result.basis[2] = alignedAxis;
    const float stretch = 1.0f + MAX_BODY_STRETCH * nearHole * activation;
    // Axial scale follows s while each perpendicular axis is squeezed by 1/sqrt(s).
    result.axialScale = stretch;
    result.perpendicularScale = 1.0f / std::sqrt(stretch);

    const float horizonFade = 1.0f - smoothBetween(EVENT_HORIZON_RADIUS * 0.65f,
                                                   EVENT_HORIZON_RADIUS * 1.4f,
                                                   postPullDistance);
    const float endStage = sun ? smoothBetween(0.985f, 1.0f, progress) : activation;
    const float swallowedAmount = std::clamp(horizonFade * endStage, 0.0f, 1.0f);
    const float remaining = 1.0f - swallowedAmount;
    result.axialScale *= remaining;
    result.perpendicularScale *= remaining;
    result.opacity = remaining;
    result.swallowed = remaining <= 0.001f;
    return result;
}

void BlackHole::draw(const glm::mat4 &view, const glm::mat4 &projection,
                     const glm::vec3 &cameraPosition, float animationTime) const {
    const float visibility = getVisibility();
    if (visibility <= 0.0f || state == BlackHoleState::Idle)
        return;

    const glm::vec3 position = getPosition();
    const glm::mat4 horizonModel =
        glm::translate(glm::mat4(1.0f), position) *
        glm::scale(glm::mat4(1.0f), glm::vec3(EVENT_HORIZON_RADIUS * visibility));
    const glm::mat4 diskModel =
        glm::translate(glm::mat4(1.0f), position) *
        glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(visibility));

    const GLboolean cullingWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendingWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthWriteWasEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWasEnabled);
    GLint oldBlendSrcRgb = GL_ONE;
    GLint oldBlendDstRgb = GL_ZERO;
    GLint oldBlendSrcAlpha = GL_ONE;
    GLint oldBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldBlendDstAlpha);

    diskShader.use();
    diskShader.set("model", diskModel);
    diskShader.set("view", view);
    diskShader.set("projection", projection);
    diskShader.set("cameraPos", cameraPosition);
    diskShader.set("time", animationTime);
    diskShader.set("opacity", visibility);
    diskShader.set("innerRadius", DISK_INNER_RADIUS);
    diskShader.set("outerRadius", DISK_OUTER_RADIUS);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    accretionDisk.draw();

    glDepthMask(depthWriteWasEnabled);
    if (cullingWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glBlendFuncSeparate(static_cast<GLenum>(oldBlendSrcRgb),
                        static_cast<GLenum>(oldBlendDstRgb),
                        static_cast<GLenum>(oldBlendSrcAlpha),
                        static_cast<GLenum>(oldBlendDstAlpha));
    if (blendingWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    horizonShader.use();
    horizonShader.set("model", horizonModel);
    horizonShader.set("view", view);
    horizonShader.set("projection", projection);
    horizonSphere.draw();
}

GLuint BlackHole::renderLensing(GLuint inputTexture, int width, int height,
                                const glm::mat4 &view, const glm::mat4 &projection,
                                bool enabled) {
    if (!initialized || !enabled || inputTexture == 0 || getVisibility() <= 0.0f || isIdle())
        return inputTexture;
    if (!resize(width, height) || lensingFramebuffer == 0)
        return inputTexture;

    GLint oldDrawFramebuffer = 0;
    GLint oldReadFramebuffer = 0;
    GLint oldViewport[4] = {0, 0, 0, 0};
    GLint oldProgram = 0;
    GLint oldVao = 0;
    GLint oldActiveTexture = 0;
    GLint oldTexture0 = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebuffer);
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullingWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendingWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthWriteWasEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWasEnabled);
    glBindFramebuffer(GL_FRAMEBUFFER, lensingFramebuffer);
    glViewport(0, 0, targetWidth, targetHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    lensingShader.use();
    lensingShader.set("sceneTexture", 0);
    lensingShader.set("view", view);
    lensingShader.set("projection", projection);
    lensingShader.set("holeCenter", getPosition());
    lensingShader.set("horizonRadius", EVENT_HORIZON_RADIUS);
    lensingShader.set("effectStrength", getVisibility());
    const GLint resolutionLocation = glGetUniformLocation(lensingShader.id, "resolution");
    glUniform2f(resolutionLocation, static_cast<float>(width), static_cast<float>(height));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glBindVertexArray(fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(oldReadFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    glDepthMask(depthWriteWasEnabled);
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (cullingWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (blendingWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture0));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
    return lensingTexture;
}
