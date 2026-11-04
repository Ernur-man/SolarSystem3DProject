#pragma once

#include "Shader.h"
#include <glad/glad.h>

/*
 * Scene hook: call beginScene(), draw the complete 3D scene, then call endScene().
 * Between endScene() and composite(), another pass can sample getHdrSceneColorTexture(),
 * render its result to a separate color texture, and pass that texture to composite().
 * The HDR texture dimensions are available through getWidth() and getHeight().
 */
class PostProcess {
public:
    ~PostProcess();

    bool initialize(int width, int height, float resolutionScale = 1.0f);
    bool resize(int width, int height, float resolutionScale);
    bool beginScene();
    void endScene();
    void composite(bool bloomEnabled, int blurIterations);
    void composite(GLuint sceneTexture, bool bloomEnabled, int blurIterations);
    void setExposure(float exposure);
    GLuint getHdrSceneColorTexture() const;
    int getWidth() const;
    int getHeight() const;
    void release();

private:
    Shader brightPassShader;
    Shader blurShader;
    Shader compositeShader;
    GLuint sceneFramebuffer = 0;
    GLuint sceneColorTexture = 0;
    GLuint sceneDepthBuffer = 0;
    GLuint pingPongFramebuffers[2] = {0, 0};
    GLuint pingPongTextures[2] = {0, 0};
    GLuint quadVao = 0;
    GLuint quadVbo = 0;
    int screenWidth = 0;
    int screenHeight = 0;
    int sceneWidth = 0;
    int sceneHeight = 0;
    int bloomWidth = 0;
    int bloomHeight = 0;
    int previousDrawFramebuffer = 0;
    int previousReadFramebuffer = 0;
    int previousViewport[4] = {0, 0, 0, 0};
    float resolutionScale = 1.0f;
    float exposure = 1.0f;
    bool initialized = false;
    bool sceneActive = false;

    bool createFramebuffers();
    void releaseFramebuffers();
    bool checkFramebuffer(const char *name) const;
    void drawFullscreen() const;
};
