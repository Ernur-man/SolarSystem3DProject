#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "ThreatInterface.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

enum class BlackHoleState { Idle, Approaching, Consuming, Returning };

struct BodyEffect {
    glm::vec3 position{0.0f};
    glm::mat3 basis{1.0f};
    float perpendicularScale = 1.0f;
    float axialScale = 1.0f;
    float opacity = 1.0f;
    bool swallowed = false;
};

class BlackHole final : public ThreatInterface {
public:
    bool initialize(int width, int height);
    bool resize(int width, int height);
    void release();

    void setThreatSpeedMultiplier(float multiplier) override;
    float getThreatProgress() const override;
    bool isSunSwallowed() const override;
    void update(float deltaTime) override;
    void reset() override;

    void summon();
    void returnBack();
    bool isIdle() const;
    BlackHoleState getState() const;
    const char *getStateName() const;
    glm::vec3 getPosition() const;
    float getVisibility() const;
    float getHorizonRadius() const;
    float getPullRadius() const;
    float getMaxPullStrength() const;
    float getMaxBodyStretch() const;
    float getStretchInfluenceRadius() const;
    float getBodyActivationThreshold() const;
    BodyEffect affectBody(const glm::vec3 &orbitPosition, bool sun) const;

    void draw(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::vec3 &cameraPosition, float animationTime) const;
    GLuint renderLensing(GLuint inputTexture, int width, int height,
                         const glm::mat4 &view, const glm::mat4 &projection,
                         bool enabled);

private:
    static constexpr float EVENT_HORIZON_RADIUS = 1.35f;
    static constexpr float DISK_INNER_RADIUS = 1.55f;
    static constexpr float DISK_OUTER_RADIUS = 5.4f;
    static constexpr float PULL_RADIUS = 8.0f;
    static constexpr float MAX_PULL_STRENGTH = 8.0f;
    static constexpr float MAX_BODY_STRETCH = 4.0f;
    static constexpr float BODY_ACTIVATION_THRESHOLD = 0.10f;
    // Defence tuning point: p advances from 0 to 1 in about 90 seconds at 1x.
    static constexpr float kSwallowSecondsAtOneX = 90.0f;

    BlackHoleState state = BlackHoleState::Idle;
    float progress = 0.0f;
    float speedMultiplier = 1.0f;
    float targetSpeedMultiplier = 1.0f;
    Shader horizonShader;
    Shader diskShader;
    Shader lensingShader;
    Mesh horizonSphere;
    Mesh accretionDisk;
    GLuint lensingFramebuffer = 0;
    GLuint lensingTexture = 0;
    GLuint fullscreenVao = 0;
    int targetWidth = 0;
    int targetHeight = 0;
    bool initialized = false;

    void setState(BlackHoleState nextState);
    bool createLensingTarget();
    void releaseLensingTarget();
    bool checkLensingFramebuffer() const;
};
