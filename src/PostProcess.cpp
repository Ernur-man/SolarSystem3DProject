#include "PostProcess.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

PostProcess::~PostProcess() {
    release();
}

bool PostProcess::initialize(int width, int height, float scale) {
    if (!brightPassShader.load("shaders/post.vert", "shaders/bright_pass.frag") ||
        !blurShader.load("shaders/post.vert", "shaders/blur.frag") ||
        !compositeShader.load("shaders/post.vert", "shaders/composite.frag")) {
        release();
        return false;
    }

    const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
    };
    glGenVertexArrays(1, &quadVao);
    glGenBuffers(1, &quadVbo);
    glBindVertexArray(quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    initialized = true;
    return resize(width, height, scale);
}

bool PostProcess::resize(int width, int height, float scale) {
    if (!initialized || width <= 0 || height <= 0 || scale <= 0.0f)
        return false;
    if (screenWidth == width && screenHeight == height && resolutionScale == scale &&
        sceneFramebuffer != 0)
        return true;

    GLint oldDrawFramebuffer = 0;
    GLint oldReadFramebuffer = 0;
    GLint oldActiveTexture = 0;
    GLint oldTexture0 = 0;
    GLint oldRenderbuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRenderbuffer);

    releaseFramebuffers();
    screenWidth = width;
    screenHeight = height;
    resolutionScale = scale;
    sceneWidth = std::max(1, static_cast<int>(static_cast<float>(width) * scale));
    sceneHeight = std::max(1, static_cast<int>(static_cast<float>(height) * scale));
    bloomWidth = std::max(1, sceneWidth / 2);
    bloomHeight = std::max(1, sceneHeight / 2);
    const bool complete = createFramebuffers();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(oldReadFramebuffer));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(oldRenderbuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture0));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));

    if (!complete) {
        releaseFramebuffers();
        sceneWidth = 0;
        sceneHeight = 0;
        bloomWidth = 0;
        bloomHeight = 0;
    }
    return complete;
}

bool PostProcess::createFramebuffers() {
    glGenFramebuffers(1, &sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sceneWidth, sceneHeight, 0, GL_RGBA, GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture,
                           0);
    glGenRenderbuffers(1, &sceneDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sceneWidth, sceneHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              sceneDepthBuffer);
    if (!checkFramebuffer("HDR scene FBO"))
        return false;

    glGenFramebuffers(2, pingPongFramebuffers);
    glGenTextures(2, pingPongTextures);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFramebuffers[i]);
        glBindTexture(GL_TEXTURE_2D, pingPongTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               pingPongTextures[i], 0);
        const char *name = i == 0 ? "Bloom ping-pong 0 FBO" : "Bloom ping-pong 1 FBO";
        if (!checkFramebuffer(name))
            return false;
    }
    return true;
}

void PostProcess::releaseFramebuffers() {
    if (sceneColorTexture != 0)
        glDeleteTextures(1, &sceneColorTexture);
    if (sceneDepthBuffer != 0)
        glDeleteRenderbuffers(1, &sceneDepthBuffer);
    if (sceneFramebuffer != 0)
        glDeleteFramebuffers(1, &sceneFramebuffer);
    if (pingPongTextures[0] != 0 || pingPongTextures[1] != 0)
        glDeleteTextures(2, pingPongTextures);
    if (pingPongFramebuffers[0] != 0 || pingPongFramebuffers[1] != 0)
        glDeleteFramebuffers(2, pingPongFramebuffers);
    sceneColorTexture = 0;
    sceneDepthBuffer = 0;
    sceneFramebuffer = 0;
    pingPongTextures[0] = 0;
    pingPongTextures[1] = 0;
    pingPongFramebuffers[0] = 0;
    pingPongFramebuffers[1] = 0;
}

bool PostProcess::checkFramebuffer(const char *name) const {
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;
    std::cerr << "Framebuffer incomplete [" << name << "]: 0x" << std::hex << status << std::dec
              << '\n';
    return false;
}

bool PostProcess::beginScene() {
    if (!initialized || sceneFramebuffer == 0 || sceneWidth <= 0 || sceneHeight <= 0 || sceneActive)
        return false;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.004f, 0.006f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    sceneActive = true;
    return true;
}

void PostProcess::endScene() {
    if (!sceneActive)
        return;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    sceneActive = false;
}

void PostProcess::composite(bool bloomEnabled, int blurIterations) {
    composite(sceneColorTexture, bloomEnabled, blurIterations);
}

void PostProcess::composite(GLuint sourceTexture, bool bloomEnabled, int blurIterations) {
    if (!initialized || sourceTexture == 0 || sceneActive || previousViewport[2] <= 0 ||
        previousViewport[3] <= 0)
        return;

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean srgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    GLboolean depthWriteEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
    GLint oldProgram = 0;
    GLint oldVao = 0;
    GLint oldActiveTexture = 0;
    GLint oldTexture0 = 0;
    GLint oldTexture1 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture1);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_FRAMEBUFFER_SRGB);

    GLuint blurredTexture = 0;
    if (bloomEnabled && blurIterations > 0 && bloomWidth > 0 && bloomHeight > 0) {
        brightPassShader.use();
        brightPassShader.set("sceneTexture", 0);
        brightPassShader.set("threshold", 0.85f);
        brightPassShader.set("softKnee", 0.5f);
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFramebuffers[0]);
        glViewport(0, 0, bloomWidth, bloomHeight);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        drawFullscreen();

        bool horizontal = true;
        GLuint blurSource = pingPongTextures[0];
        blurShader.use();
        blurShader.set("image", 0);
        const GLint texelSizeLocation = glGetUniformLocation(blurShader.id, "texelSize");
        glUniform2f(texelSizeLocation, 1.0f / bloomWidth, 1.0f / bloomHeight);
        for (int i = 0; i < blurIterations * 2; ++i) {
            const int target = horizontal ? 1 : 0;
            glBindFramebuffer(GL_FRAMEBUFFER, pingPongFramebuffers[target]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, blurSource);
            blurShader.set("horizontal", horizontal ? 1 : 0);
            drawFullscreen();
            blurSource = pingPongTextures[target];
            horizontal = !horizontal;
        }
        blurredTexture = blurSource;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    compositeShader.use();
    compositeShader.set("sceneTexture", 0);
    compositeShader.set("bloomTexture", 1);
    compositeShader.set("bloomEnabled", bloomEnabled && blurredTexture != 0 ? 1 : 0);
    compositeShader.set("exposure", exposure);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurredTexture);
    drawFullscreen();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glDepthMask(depthWriteEnabled);
    if (depthEnabled)
        glEnable(GL_DEPTH_TEST);
    if (cullEnabled)
        glEnable(GL_CULL_FACE);
    if (blendEnabled)
        glEnable(GL_BLEND);
    if (srgbEnabled)
        glEnable(GL_FRAMEBUFFER_SRGB);
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture1));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
}

void PostProcess::setExposure(float value) {
    exposure = std::max(value, 0.0f);
}

GLuint PostProcess::getHdrSceneColorTexture() const {
    return sceneColorTexture;
}

int PostProcess::getWidth() const {
    return sceneWidth;
}

int PostProcess::getHeight() const {
    return sceneHeight;
}

void PostProcess::release() {
    releaseFramebuffers();
    if (quadVbo != 0)
        glDeleteBuffers(1, &quadVbo);
    if (quadVao != 0)
        glDeleteVertexArrays(1, &quadVao);
    if (brightPassShader.id != 0)
        glDeleteProgram(brightPassShader.id);
    if (blurShader.id != 0)
        glDeleteProgram(blurShader.id);
    if (compositeShader.id != 0)
        glDeleteProgram(compositeShader.id);
    quadVbo = 0;
    quadVao = 0;
    brightPassShader.id = 0;
    blurShader.id = 0;
    compositeShader.id = 0;
    initialized = false;
}

void PostProcess::drawFullscreen() const {
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
