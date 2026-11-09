#pragma once

#include <glad/glad.h>
#include <string>

class Texture {
public:
    ~Texture();
    bool load(const std::string &path, bool srgb = true,
              const char *fallbackDescription = "shader fallback");
    bool isLoaded() const {
        return textureId != 0;
    }
    GLuint id() const { return textureId; }
    void bind(GLuint unit = 0) const;
    void release();

private:
    GLuint textureId = 0;
};
